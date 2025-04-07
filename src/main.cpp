#include <iostream>
#include <vector>
#include <complex>
#include <cmath>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <mutex>
#include <deque>
#include <iomanip>
#include <fcntl.h>
#include <memory>
#include <future>

#include "radio/rtl_device.h"
#include "audio/demodulator.h"
#include "audio/mp3_encoder.h"
#include "streaming/icecast_client.h"
#include "config/config.h"
#include "radio/scanner.h"
#include "ui/status_display.h"

// Forward declarations for liquid-dsp types
struct msresamp_rrrf_s;
typedef struct msresamp_rrrf_s * msresamp_rrrf;

struct iirfilt_crcf_s;
typedef struct iirfilt_crcf_s * iirfilt_crcf;

// Function declarations for liquid-dsp
extern "C" {
    msresamp_rrrf msresamp_rrrf_create(float rate, float As);
    void msresamp_rrrf_destroy(msresamp_rrrf q);
    void msresamp_rrrf_execute(msresamp_rrrf q, const float *x, unsigned int nx, float *y, unsigned int *ny);
    
    iirfilt_crcf iirfilt_crcf_create_lowpass(unsigned int order, float fc);
    void iirfilt_crcf_destroy(iirfilt_crcf q);
    void iirfilt_crcf_execute(iirfilt_crcf q, std::complex<float> x, std::complex<float> *y);
}

// Global signal handling
std::atomic<bool> g_running{true};

// Audio buffer
std::mutex g_buffer_mutex;
std::deque<float> g_audio_buffer;

// MP3 buffer
std::mutex g_mp3_buffer_mutex;
struct MP3Chunk {
    std::vector<unsigned char> data;
    size_t size;
};
std::deque<MP3Chunk> g_mp3_queue;

// Signal handler
void signal_handler(int sig) {
    if (sig == SIGPIPE) {
        std::cerr << "Caught SIGPIPE - connection broken\n";
    } else if (sig == SIGINT) {
        std::cout << "Caught SIGINT - shutting down\n";
        g_running = false;
        
        // Force exit after a short delay if the application doesn't shut down cleanly
        static bool force_exit_scheduled = false;
        if (!force_exit_scheduled) {
            force_exit_scheduled = true;
            std::thread([]{
                std::this_thread::sleep_for(std::chrono::seconds(3));
                std::cerr << "Forcing exit after timeout\n";
                exit(1);
            }).detach();
        }
    } else {
        std::cerr << "Caught signal " << sig << "\n";
        g_running = false;
    }
}

class Application {
private:
    Config config;
    std::unique_ptr<RTLDevice> rtl_device;
    std::unique_ptr<FMDemodulator> fm_demodulator;
    std::unique_ptr<AMDemodulator> am_demodulator;
    std::unique_ptr<AudioFilter> lowcut_filter;
    std::unique_ptr<MP3Encoder> mp3_encoder;
    std::unique_ptr<IcecastClient> icecast_client;
    std::unique_ptr<Scanner> scanner;
    std::unique_ptr<StatusDisplay> status_display;
    
    std::thread rtl_thread;
    std::thread icecast_thread;
    std::thread status_thread;
    
    std::atomic<float> signal_strength{0.0f};
    std::atomic<bool> squelch_active{false};
    std::chrono::steady_clock::time_point last_signal_above_threshold;
    
    // Resampler
    msresamp_rrrf resampler;
    
    // Channel filter
    iirfilt_crcf filter;
    
public:
    Application() : resampler(nullptr), filter(nullptr) {
        // Initialize signal handlers
        signal(SIGINT, signal_handler);
        signal(SIGPIPE, signal_handler);
    }
    
    ~Application() {
        shutdown();
    }
    
    bool initialize(const std::string& config_file, bool quiet_mode = false) {
        try {
            if (!config_file.empty()) {
                config.loadFromFile(config_file);
            }
            
            // Initialize libshout
            IcecastClient::initializeLibrary();
            
            // Create RTL device
            rtl_device.reset(new RTLDevice());
            
            // Create demodulators
            fm_demodulator.reset(new FMDemodulator(config.getSampleRate(), config.getMode()));
            am_demodulator.reset(new AMDemodulator());
            
            // Create lowcut filter
            lowcut_filter.reset(new AudioFilter(config.getLowcutFreq(), config.getLowcutOrder(), config.getAudioRate()));
            lowcut_filter->setEnabled(config.isLowcutEnabled());
            
            // Create MP3 encoder
            mp3_encoder.reset(new MP3Encoder(config.getAudioRate(), config.getMp3Bitrate(), config.getMp3Quality()));
            
            // Create Icecast client
            icecast_client.reset(new IcecastClient(
                config.getIcecastHost(), 
                config.getIcecastPort(), 
                config.getIcecastMount(), 
                config.getIcecastPassword(), 
                config.getIcecastUser(),
                config.getIcecastProtocol(),
                config.getIcecastFormat()
            ));
            
            icecast_client->setReconnectAttempts(config.getReconnectAttempts());
            icecast_client->setReconnectDelay(config.getReconnectDelayMs());
            icecast_client->setStationTitle(config.getIcecastStationTitle());
            
            // Create scanner if enabled
            if (config.isScanEnabled() && !config.getScanlist().empty()) {
                scanner.reset(new Scanner(config.getScanlist()));
                scanner->setStepDelay(config.getStepDelayMs());
            }
            
            // Create status display
            status_display.reset(new StatusDisplay(quiet_mode));
            
            // Initialize resampler
            // Stop-band attenuation is 60 dB
            resampler = msresamp_rrrf_create(
                static_cast<float>(config.getAudioRate()) / config.getSampleRate(), // Resampling ratio
                60.0f // Stop-band attenuation
            );
            
            // Initialize channel filter based on mode
            initModulation(config.getMode());
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Initialization error: " << e.what() << std::endl;
            return false;
        }
    }
    
    void start() {
        std::cout << "[DEBUG] Starting application" << std::endl;
        
        if (!rtl_device || !rtl_device->open(0)) {
            std::cerr << "[ERROR] Failed to open RTL-SDR device" << std::endl;
            return;
        }
        
        // Configure RTL device
        rtl_device->set_sample_rate(config.getSampleRate());
        rtl_device->set_center_freq(config.getCenterFreq());
        rtl_device->set_gain_mode(config.getGainMode());
        if (config.getGainMode() == 1) {
            rtl_device->set_tuner_gain(config.getTunerGain());
        }
        
        // Update the status display with the current frequency
        status_display->updateFrequency(config.getCenterFreq());
        
        rtl_device->set_ppm_correction(config.getPpmCorrection());

        // Set RTL callback
        rtl_device->set_callback([this](unsigned char* buf, uint32_t len) {
            processRtlData(buf, len);
        });
        
        // Start scanner if enabled
        if (scanner && config.isScanEnabled()) {
            scanner->start();
        }
        
        // Start threads
        rtl_thread = std::thread(&Application::rtlThreadFunction, this);
        icecast_thread = std::thread(&Application::icecastThreadFunction, this);
        status_thread = std::thread(&Application::statusThreadFunction, this);
        
        std::cout << "[DEBUG] Application started" << std::endl;
    }
    
    void shutdown() {
        std::cout << "[DEBUG] Shutting down application" << std::endl;
        g_running = false;
        
        // Stop scanner if active
        if (scanner) {
            scanner->stop();
        }
        
        // Stop RTL device streaming
        if (rtl_device) {
            rtl_device->stop_streaming();
        }
        
        std::cout << "[DEBUG] Waiting for threads to finish" << std::endl;
        
        // Join threads with timeout
        auto join_with_timeout = [](std::thread& t, const char* name, int timeout_ms) {
            if (!t.joinable()) return;
            
            std::cout << "[DEBUG] Waiting for " << name << " thread to finish" << std::endl;
            
            auto future = std::async(std::launch::async, [&t]() {
                t.join();
            });
            
            if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
                std::cerr << "[ERROR] Timeout waiting for " << name << " thread" << std::endl;
                // We can't safely detach or terminate the thread, so we'll just continue
            } else {
                std::cout << "[DEBUG] " << name << " thread finished" << std::endl;
            }
        };
        
        // Join threads with a 2-second timeout
        join_with_timeout(rtl_thread, "RTL", 2000);
        join_with_timeout(icecast_thread, "Icecast", 2000);
        join_with_timeout(status_thread, "Status", 2000);
        
        // Clean up resources
        if (rtl_device) {
            rtl_device->close();
        }
        
        if (icecast_client) {
            icecast_client->disconnect();
        }
        
        // Clean up liquid-dsp resources
        if (resampler) {
            msresamp_rrrf_destroy(resampler);
            resampler = nullptr;
        }
        
        if (filter) {
            iirfilt_crcf_destroy(filter);
            filter = nullptr;
        }
        
        // Shutdown libshout
        IcecastClient::shutdownLibrary();
        
        std::cout << "[DEBUG] Shutdown complete" << std::endl;
    }
    
    void processRtlData(unsigned char* buf, uint32_t len) {
        if (!g_running) {
            return;
        }
        
        // Check if we need to change frequency (scanner mode)
        if (scanner && scanner->isActive()) {
            double new_freq = scanner->nextChannel();
            if (new_freq > 0.0) {
                rtl_device->set_center_freq(new_freq);
                status_display->updateFrequency(new_freq);
                status_display->updateScannerStatus(true, scanner->getCurrentName());
            }
        }
        
        // Convert buffer to complex samples
        std::vector<std::complex<float>> iq_samples;
        iq_samples.reserve(len / 2);
        
        for (uint32_t i = 0; i < len; i += 2) {
            float i_sample = (buf[i] - 127.5f) / 127.5f;
            float q_sample = (buf[i+1] - 127.5f) / 127.5f;
            iq_samples.push_back(std::complex<float>(i_sample, q_sample));
        }
        
        // Apply channel filter
        std::vector<std::complex<float>> filtered_samples;
        filtered_samples.reserve(iq_samples.size());
        
        for (const auto& sample : iq_samples) {
            std::complex<float> filtered_sample;
            iirfilt_crcf_execute(filter, sample, &filtered_sample);
            filtered_samples.push_back(filtered_sample);
        }
        
        // Calculate signal strength (in dB)
        float sum_squared = 0.0f;
        for (const auto& sample : filtered_samples) {
            sum_squared += std::norm(sample);
        }
        float rms = std::sqrt(sum_squared / filtered_samples.size());
        float signal_db = 20.0f * std::log10(rms + 1e-10f);
        signal_strength = signal_db;
        
        // Update status display
        status_display->updateSignalLevel(signal_db);
        
        // Check squelch
        bool signal_present = true;
        if (config.isSquelchEnabled()) {
            if (signal_db < config.getSquelchThreshold()) {
                if (!squelch_active) {
                    squelch_active = true;
                    status_display->updateSquelchStatus(true);
                }
            } else {
                last_signal_above_threshold = std::chrono::steady_clock::now();
                if (squelch_active) {
                    squelch_active = false;
                    status_display->updateSquelchStatus(false);
                }
            }
            
            signal_present = !squelch_active;
        }
        
        // Demodulate
        std::vector<float> audio_samples;
        audio_samples.reserve(filtered_samples.size());
        
        if (signal_present) {
            if (config.getMode() == ModulationMode::AM_MODE) {
                // AM demodulation
                for (const auto& sample : filtered_samples) {
                    audio_samples.push_back(am_demodulator->demodulate(sample));
                }
            } else {
                // FM demodulation
                for (size_t i = 1; i < filtered_samples.size(); i++) {
                    audio_samples.push_back(
                        fm_demodulator->demodulate(filtered_samples[i-1], filtered_samples[i])
                    );
                }
            }
        } else {
            // If squelch is active, output silence
            audio_samples.resize(filtered_samples.size(), 0.0f);
        }
        
        // Resample to audio rate
        unsigned int num_written;
        std::vector<float> resampled_audio(audio_samples.size() * 2); // Allocate extra space
        
        msresamp_rrrf_execute(resampler, audio_samples.data(), audio_samples.size(),
                             resampled_audio.data(), &num_written);
        
        resampled_audio.resize(num_written);
        
        // Apply lowcut filter if enabled
        if (lowcut_filter->isEnabled()) {
            for (auto& sample : resampled_audio) {
                sample = lowcut_filter->process(sample);
            }
        }
        
        // Add to audio buffer
        {
            std::lock_guard<std::mutex> lock(g_buffer_mutex);
            g_audio_buffer.insert(g_audio_buffer.end(), resampled_audio.begin(), resampled_audio.end());
            
            // Update buffer status
            float buffer_seconds = static_cast<float>(g_audio_buffer.size()) / config.getAudioRate();
            status_display->updateBufferStatus(buffer_seconds);
        }
    }
    
    void rtlThreadFunction() {
        try {
            rtl_device->start_streaming();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] RTL thread error: " << e.what() << std::endl;
            g_running = false;
        }
    }
    
    void icecastThreadFunction() {
        try {
            // Connect to Icecast server
            bool icecast_available = icecast_client->connect();
            
            if (!icecast_available) {
                std::cerr << "[ERROR] Failed to connect to Icecast server" << std::endl;
                std::cout << "[DEBUG] Continuing in offline mode (no streaming)" << std::endl;
                status_display->updateConnectionStatus(false);
            } else {
             
                status_display->updateConnectionStatus(true);
            }
            
            // Chunk size for MP3 encoding
            const size_t CHUNK_SIZE = config.getAudioRate() * config.getAudioBufferSeconds();
            
            // Timestamp for metadata updates
            auto last_metadata_update = std::chrono::steady_clock::now();
            const int METADATA_UPDATE_INTERVAL_SEC = 10;
            
            // Timestamp for reconnection attempts
            auto last_reconnect_attempt = std::chrono::steady_clock::now();
            const int RECONNECT_INTERVAL_SEC = 30;  // Try to reconnect every 30 seconds if not connected
            
            while (g_running) {
                // Check connection status if we were previously connected
                if (icecast_available && !icecast_client->checkConnection()) {
                    std::cout << "[DEBUG] Icecast connection lost" << std::endl;
                    status_display->updateConnectionStatus(false);
                    icecast_available = false;
                }
                
                // Try to reconnect periodically if not connected
                if (!icecast_available) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_attempt).count() 
                        >= RECONNECT_INTERVAL_SEC) {
                        
                        std::cout << "[DEBUG] Attempting to reconnect to Icecast" << std::endl;
                        last_reconnect_attempt = now;
                        
                        if (icecast_client->reconnect()) {
                            std::cout << "[DEBUG] Reconnected to Icecast server" << std::endl;
                            status_display->updateConnectionStatus(true);
                            icecast_available = true;
                        } else {
                            std::cerr << "[ERROR] Failed to reconnect to Icecast server" << std::endl;
                        }
                    }
                }
                
                // Get audio samples from buffer
                std::vector<float> audio_chunk;
                {
                    std::lock_guard<std::mutex> lock(g_buffer_mutex);
                    
                    // If buffer has enough samples, take a chunk
                    if (g_audio_buffer.size() >= CHUNK_SIZE) {
                        audio_chunk.assign(g_audio_buffer.begin(), g_audio_buffer.begin() + CHUNK_SIZE);
                        g_audio_buffer.erase(g_audio_buffer.begin(), g_audio_buffer.begin() + CHUNK_SIZE);
                    }
                }
                
                // If we have audio to process and we're connected to Icecast
                if (!audio_chunk.empty() && icecast_available) {
                    try {
                        // Encode to MP3
                        std::vector<unsigned char> mp3_data = mp3_encoder->encode(audio_chunk);
                        
                        // Send to Icecast
                        if (!mp3_data.empty()) {
                            if (icecast_client->sendData(mp3_data)) {
                                status_display->updatePacketSize(mp3_data.size());
                            } else {
                                std::cerr << "[ERROR] Failed to send MP3 data" << std::endl;
                                icecast_available = false;
                                status_display->updateConnectionStatus(false);
                            }
                        }
                        
                        // Update metadata periodically
                        auto now = std::chrono::steady_clock::now();
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_metadata_update).count() 
                            >= METADATA_UPDATE_INTERVAL_SEC) {
                            
                            last_metadata_update = now;
                            icecast_client->updateMetadata(rtl_device->get_center_freq(), signal_strength);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] MP3 encoding/streaming error: " << e.what() << std::endl;
                    }
                } else if (!audio_chunk.empty()) {
                    // We have audio but no Icecast connection - just discard it
                } else {
                    // No audio to process, sleep a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            
            // Disconnect from Icecast server if we were connected
            if (icecast_available) {
                std::cout << "[DEBUG] Disconnecting from Icecast server" << std::endl;
                icecast_client->disconnect();
                status_display->updateConnectionStatus(false);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Icecast thread error: " << e.what() << std::endl;
        }
    }
    
    void statusThreadFunction() {
        while (g_running) {
            status_display->display();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // Control functions
    void changeFrequency(double freq_mhz) {
        if (rtl_device) {
            rtl_device->set_center_freq(freq_mhz);
            status_display->updateFrequency(freq_mhz);
        }
    }
    
    void setModulation(ModulationMode mode) {
        config.setMode(mode);
        initModulation(mode);
        status_display->updateMode(config.getModeString());
    }
    
    void toggleSquelch() {
        config.setSquelchEnabled(!config.isSquelchEnabled());
        squelch_active = false;
        status_display->updateSquelchStatus(false);
    }
    
    void setSquelchThreshold(float threshold) {
        config.setSquelchThreshold(threshold);
    }
    
    void toggleLowcutFilter() {
        if (lowcut_filter) {
            lowcut_filter->setEnabled(!lowcut_filter->isEnabled());
            status_display->updateLowcutStatus(lowcut_filter->isEnabled());
        }
    }
    
    void setLowcutFrequency(float freq) {
        if (lowcut_filter) {
            lowcut_filter->setCutoffFrequency(freq);
            config.setLowcutFreq(freq);
        }
    }
    
    void toggleScanner() {
        if (scanner) {
            if (scanner->isActive()) {
                scanner->stop();
                status_display->updateScannerStatus(false);
            } else {
                scanner->start();
                status_display->updateScannerStatus(true, scanner->getCurrentName());
            }
        }
    }
    
    void initModulation(ModulationMode mode) {
        // Set up channel filter based on mode
        float fc;  // Normalized cutoff frequency
        
        switch (mode) {
            case ModulationMode::WFM_MODE:
                fc = 120000.0f / config.getSampleRate(); // 120 kHz for WFM
                break;
            case ModulationMode::NFM_MODE:
                fc = 12500.0f / config.getSampleRate(); // 12.5 kHz for NFM
                break;
            case ModulationMode::AM_MODE:
                fc = 8000.0f / config.getSampleRate(); // 8 kHz for AM
                break;
            default:
                fc = 120000.0f / config.getSampleRate(); // Default to WFM
        }
        
        // Create filter if it doesn't exist, or destroy and recreate if it does
        if (filter) {
            iirfilt_crcf_destroy(filter);
        }
        
        // Create new filter
        filter = iirfilt_crcf_create_lowpass(8, fc);
        
        // Update demodulator
        fm_demodulator->setMode(mode, config.getSampleRate());
    }
};

void print_usage() {
    std::cout << "Usage: rtl_icecast [options]\n"
              << "  -f <freq>      Center frequency in MHz (default: 99.9)\n"
              << "  -g <gain>      Tuner gain in tenths of dB (default: auto)\n"
              << "  -p <ppm>       Frequency correction in ppm\n"
              << "  -m <mode>      Modulation mode: wfm, nfm, am (default: wfm)\n"
              << "  -s <host>      Icecast server hostname\n"
              << "  -P <port>      Icecast server port\n"
              << "  -M <mount>     Icecast mount point\n"
              << "  -u <user>      Icecast username\n"
              << "  -w <pass>      Icecast password\n"
              << "  -n <name>      Station name\n"
              << "  -c <file>      Configuration file\n"
              << "  -q             Quiet mode (no status output)\n"
              << "  -h             Show this help message\n";
}

int main(int argc, char* argv[]) {
    Config config;
    std::string config_file = "config.ini";  // Default to config.ini in the current directory
    bool quiet_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-f" && i + 1 < argc) {
            config.setCenterFreq(std::stod(argv[++i]));
        } else if (arg == "-g" && i + 1 < argc) {
            config.setGainMode(1);
            config.setTunerGain(std::stoi(argv[++i]));
        } else if (arg == "-p" && i + 1 < argc) {
            config.setPpmCorrection(std::stoi(argv[++i]));
        } else if (arg == "-m" && i + 1 < argc) {
            config.setMode(Config::stringToMode(argv[++i]));
        } else if (arg == "-s" && i + 1 < argc) {
            config.setIcecastHost(argv[++i]);
        } else if (arg == "-P" && i + 1 < argc) {
            config.setIcecastPort(std::stoi(argv[++i]));
        } else if (arg == "-M" && i + 1 < argc) {
            config.setIcecastMount(argv[++i]);
        } else if (arg == "-u" && i + 1 < argc) {
            config.setIcecastUser(argv[++i]);
        } else if (arg == "-w" && i + 1 < argc) {
            config.setIcecastPassword(argv[++i]);
        } else if (arg == "-n" && i + 1 < argc) {
            config.setIcecastStationTitle(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "-q") {
            quiet_mode = true;
        } else if (arg == "-h") {
            print_usage();
            return 0;
        }
    }
    
    // Create and initialize application
    Application app;
    
    if (!app.initialize(config_file, quiet_mode)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    // Start application
    app.start();
    
    // Wait for application to finish
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
