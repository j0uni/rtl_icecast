#include <iostream>
#include <vector>
#include <complex>
#include <rtl-sdr.h>
#include <liquid/liquid.h>
#include <lame/lame.h>
#include <shout/shout.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <mutex>
#include <deque>
#include <iomanip>
#include <fcntl.h>
#include "config.h"
#include "scanner.h"

// Global configuration
Config g_config;

// FM mode selection
enum class FMMode {
    NARROW,
    WIDE,
};

// Global RTL-SDR device pointer
rtlsdr_dev_t *g_dev = nullptr;

Scanner *scanner = nullptr;

// Squelch state
std::atomic<bool> squelch_active{false};
std::chrono::steady_clock::time_point last_signal_above_threshold;

// Audio filter state
iirfilt_rrrf lowcut_filter = nullptr;  // Low-cut filter

#define SAMPLE_RATE 1024000  // 1.024 MHz
#define AUDIO_RATE 48000     // 48 kHz
#define CENTER_FREQ 99.9e6   // 99.9 MHz
#define RTL_READ_SIZE (16 * 16384)

#define NFM_DEVIATION 12500    
#define NFM_FILTER_BW 12500*2  

#define WFM_DEVIATION 75000   // 75 kHz for WBFM
#define WFM_FILTER_BW 120000  // 120 kHz for WFM

#define AM_FILTER_BW  8000    // 8 kHz for AM

#define AUDIO_BUFFER_IN_SECONDS 2
#define CHUNK_SIZE (AUDIO_RATE * AUDIO_BUFFER_IN_SECONDS)  // 10 seconds of audio
#define MP3_BUFFER_SIZE (CHUNK_SIZE * 2)  // Plenty of space for MP3 data

std::atomic<bool> running{true};
std::atomic<bool> icecast_connected{false};  // Track Icecast connection state
std::mutex buffer_mutex;
std::deque<float> audio_buffer;  // Growing buffer for audio samples
std::chrono::steady_clock::time_point last_stats_time;
msresamp_rrrf resampler;  // Make resampler global so callback can access it
std::complex<float> prev_sample(1.0f, 0.0f);  // Make prev_sample global for the callback
iirfilt_crcf filter;  // FM channel filter
ModulationMode current_mode = ModulationMode::WFM_MODE; // Default to wide FM

// Structure to hold MP3 data
struct MP3Chunk {
    std::vector<unsigned char> data;
    size_t size;
};

std::mutex mp3_buffer_mutex;
std::deque<MP3Chunk> mp3_queue;
const size_t MAX_MP3_QUEUE_SIZE = 10;  // Maximum number of MP3 chunks to queue

// Status tracking
std::atomic<size_t> last_packet_size{0};
std::atomic<float> signal_strength{0.0f};
std::mutex status_mutex;
struct StatusInfo {
    float buffer_seconds{0};
    float signal_level_db{-120.0f};
    size_t packet_size{0};
    bool connected{false};
};
StatusInfo current_status;

// Timestamp for metadata updates
std::chrono::steady_clock::time_point last_metadata_update;
const int METADATA_UPDATE_INTERVAL_SEC = 10; // Update metadata every 10 seconds

void signal_handler(int sig) {
    if (sig == SIGPIPE) {
        std::cerr << "Caught SIGPIPE - connection broken\n";
        icecast_connected = false;  // Mark connection as broken
    } else if (sig == SIGINT) {
        std::cout << "Caught SIGINT - shutting down\n";
        running = false;
    } else {
        std::cerr << "Caught signal " << sig << "\n";
        running = false;
    }
}

// Advanced FM demodulator with phase unwrapping, DC blocking and de-emphasis
class FMDemodulator {
private:
    std::complex<float> prev_sample;
    float prev_demod;
    float dc_block_alpha;
    float dc_avg;
    float deemph_alpha;
    float deemph_prev;
    bool use_deemphasis;
    float deviation;
    float sample_rate;
    
public:
    FMDemodulator() : 
        prev_sample(1.0f, 0.0f),
        prev_demod(0.0f),
        dc_block_alpha(0.01f),
        dc_avg(0.0f),
        deemph_alpha(0.0f),
        deemph_prev(0.0f),
        use_deemphasis(true),
        deviation(WFM_DEVIATION),
        sample_rate(SAMPLE_RATE) {
        // Initialize de-emphasis filter
        // Time constant is 75µs for US FM broadcasts, 50µs for Europe
        // For 75 µs (used in North America), the cutoff frequency is ~2.12 kHz.
        // For 50 µs (used in Europe), the cutoff frequency is ~3.18 kHz.
        float time_constant = 75e-6f; // 75µs for US
        deemph_alpha = 1.0f - expf(-1.0f / (time_constant * sample_rate));
    }
    
    void setMode(ModulationMode mode, float sampleRate) {
        deviation = (mode == ModulationMode::WFM_MODE) ? WFM_DEVIATION : NFM_DEVIATION;
        sample_rate = sampleRate;
        
        // Update de-emphasis filter for new sample rate
        float time_constant = (mode == ModulationMode::WFM_MODE) ? 75e-6f : 50e-6f; // 75µs for WFM, 50µs for NFM
        deemph_alpha = 1.0f - expf(-1.0f / (time_constant * sample_rate));
        
        // Use de-emphasis only for wide FM
        use_deemphasis = (mode == ModulationMode::NFM_MODE);

    }
    
    void reset() {
        prev_sample = std::complex<float>(1.0f, 0.0f);
        prev_demod = 0.0f;
        dc_avg = 0.0f;
        deemph_prev = 0.0f;
    }
    
    float demodulate(std::complex<float> sample) {
        // Phase difference demodulation with unwrapping
        float phase = std::arg(sample * std::conj(prev_sample));
        
        // Phase unwrapping for large frequency deviations
        if (phase > M_PI) phase -= 2.0f * M_PI;
        else if (phase < -M_PI) phase += 2.0f * M_PI;
        
        // Convert phase to audio sample
        float demod = phase * (sample_rate / (2.0f * M_PI * deviation));
        
        // DC blocking filter
        dc_avg = dc_avg * (1.0f - dc_block_alpha) + demod * dc_block_alpha;
        float dc_blocked = demod - dc_avg;
        
        // De-emphasis filter (1st order IIR low-pass)
        float result;
        if (use_deemphasis) {
            deemph_prev = deemph_prev + deemph_alpha * (dc_blocked - deemph_prev);
            result = deemph_prev;
        } else {
            result = dc_blocked;
        }
        
        // Update state for next sample
        prev_sample = sample;
        prev_demod = demod;
        
        return result;
    }
    
    // For compatibility with the old interface
    float demodulate(std::complex<float> prev, std::complex<float> curr) {
        prev_sample = prev;
        return demodulate(curr);
    }
};

// Global demodulator instance
FMDemodulator g_demodulator;

// Wrapper function for backward compatibility
float fm_demod(std::complex<float> prev, std::complex<float> curr) {
    return g_demodulator.demodulate(prev, curr);
}

// Initialize modulation mode
void init_modulation(ModulationMode mode) {
    current_mode = mode;
    
    float cutoff_freq;
    if (mode == ModulationMode::WFM_MODE) cutoff_freq = WFM_FILTER_BW;
    else if (mode == ModulationMode::NFM_MODE) cutoff_freq = NFM_FILTER_BW;
    else cutoff_freq = AM_FILTER_BW;

    float filter_bw = cutoff_freq / (float)g_config.sample_rate;
    
    if (filter) {
        iirfilt_crcf_destroy(filter);
    }
    
    filter = iirfilt_crcf_create_prototype(
        LIQUID_IIRDES_BUTTER,
        LIQUID_IIRDES_LOWPASS,
        LIQUID_IIRDES_SOS,
        5,
        filter_bw,
        0.0f,
        1.0f,
        60.0f
    );
    
    if ((mode == ModulationMode::NFM_MODE) || (mode == ModulationMode::WFM_MODE)) {
        // Initialize the FM demodulator with the new mode
        g_demodulator.setMode(mode, g_config.sample_rate);
        g_demodulator.reset();
        
        printf("Initialized %s FM mode with %d Hz deviation and %.1f kHz filter\n",
            (mode == ModulationMode::WFM_MODE) ? "Wide" : "Narrow",
            (mode == ModulationMode::WFM_MODE) ? WFM_DEVIATION : NFM_DEVIATION,
            cutoff_freq / 1000.0f);
    }
    if (mode == ModulationMode::AM_MODE) {
        printf("Initialized AM mode with %.1f kHz filter\n", cutoff_freq / 1000.0f);
    }
}

// Initialize low-cut filter
void init_lowcut_filter() {
    // Clean up existing filter if any
    if (lowcut_filter) {
        iirfilt_rrrf_destroy(lowcut_filter);
        lowcut_filter = nullptr;
    }
    
    if (g_config.lowcut_enabled) {
        // Calculate normalized cutoff frequency
        float cutoff_norm = g_config.lowcut_freq / (float)g_config.audio_rate;
        
        // Create new filter
        lowcut_filter = iirfilt_rrrf_create_prototype(
            LIQUID_IIRDES_BUTTER,      // Butterworth filter type
            LIQUID_IIRDES_HIGHPASS,    // High-pass filter (low-cut)
            LIQUID_IIRDES_SOS,         // Second-order sections
            g_config.lowcut_order,     // Filter order
            cutoff_norm,               // Normalized cutoff frequency
            0.0f,                      // Unused for high-pass
            1.0f,                      // Pass-band ripple (unused for Butterworth)
            60.0f                      // Stop-band attenuation
        );
        
        printf("Initialized low-cut filter at %.1f Hz (order %d)\n", 
               g_config.lowcut_freq, g_config.lowcut_order);
    } else {
        printf("Low-cut filter disabled\n");
    }
}

// Function to toggle low-cut filter
void toggle_lowcut_filter() {
    g_config.lowcut_enabled = !g_config.lowcut_enabled;
    init_lowcut_filter();
    std::cout << "Low-cut filter " << (g_config.lowcut_enabled ? "enabled" : "disabled") 
              << " (cutoff: " << g_config.lowcut_freq << " Hz)" << std::endl;
}

// Function to set low-cut frequency
void set_lowcut_frequency(float freq) {
    if (freq < 20.0f || freq > 2000.0f) {
        std::cerr << "Invalid low-cut frequency. Must be between 20 Hz and 2000 Hz.\n";
        return;
    }
    
    g_config.lowcut_freq = freq;
    if (g_config.lowcut_enabled) {
        init_lowcut_filter();
    }
    std::cout << "Low-cut frequency set to " << freq << " Hz" << std::endl;
}


// Function to check Icecast connection status
bool check_icecast_connection(shout_t* shout) {
    if (!shout) {
        return false;
    }
    
    int err = shout_get_connected(shout);
    
    // SHOUTERR_CONNECTED (0) means connected
    // Any other value means not connected
    if (err == SHOUTERR_CONNECTED) {
        return true;
    }
    
    // If we thought we were connected but aren't, update the status
    if (icecast_connected.load()) {
        std::cerr << "Icecast connection lost: " << shout_get_error(shout) << std::endl;
        icecast_connected = false;
    }
    
    return false;
}


// Function to print buffer statistics
void print_buffer_stats() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 1) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        float buffer_seconds = static_cast<float>(audio_buffer.size()) / g_config.audio_rate;
        std::cout << "Buffer size: " << buffer_seconds << " seconds\n";
        last_stats_time = now;
    }
}

// Callback function to receive IQ samples
void rtl_callback(unsigned char *buf, uint32_t len, void *) {
    // Calculate signal strength (RMS of I/Q samples)
    float sum_squared = 0.0f;
    std::vector<std::complex<float>> filtered_samples(len/2);
    
    // Convert samples and apply filtering
    for (uint32_t i = 0; i < len; i += 2) {
        float i_sample = (buf[i] - 127.5f) / 127.5f;
        float q_sample = (buf[i + 1] - 127.5f) / 127.5f;
        std::complex<float> sample(i_sample, q_sample);
        
        // Apply FM channel filter
        std::complex<float> filtered;
        iirfilt_crcf_execute(filter, sample, &filtered);
        filtered_samples[i/2] = filtered;
        
        sum_squared += std::norm(filtered);
    }
    
    float rms = std::sqrt(sum_squared / (len/2));
    float db = 20 * std::log10(rms + 1e-10);
    signal_strength.store(db);
    
    // Check squelch
    bool is_squelched = false;
    if (g_config.squelch_enabled) {
        auto now = std::chrono::steady_clock::now();
        if (db >= g_config.squelch_threshold) {
            // Signal is above threshold, update the timestamp
            last_signal_above_threshold = now;
            squelch_active = false;
        } else {
            // Check if we're within the hold time
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_signal_above_threshold).count();
            if (elapsed > g_config.squelch_hold_time) {
                squelch_active = true;
                is_squelched = true;
            }
        }
    }
    
    // Process IQ samples
    std::vector<float> demod_buffer(len / 2);
    for (uint32_t i = 0; i < len/2; i++) {
        if (current_mode == ModulationMode::AM_MODE) demod_buffer[i] = std::abs(filtered_samples[i]);
        else demod_buffer[i] = fm_demod(prev_sample, filtered_samples[i]);
        prev_sample = filtered_samples[i];
    }
    
    // Resample to audio rate
    std::vector<float> resampled_buffer(static_cast<size_t>(2.0f * demod_buffer.size()));  // Extra space for 1% more samples
    unsigned int num_written;
    msresamp_rrrf_execute(resampler,
                         demod_buffer.data(),
                         demod_buffer.size(),
                         resampled_buffer.data(),
                         &num_written);
    
    // Apply low-cut filter if enabled
    if (g_config.lowcut_enabled && lowcut_filter && !is_squelched) {
        for (unsigned int i = 0; i < num_written; i++) {
            float filtered_sample;
            iirfilt_rrrf_execute(lowcut_filter, resampled_buffer[i], &filtered_sample);
            resampled_buffer[i] = filtered_sample;
        }
    }
    
    // Add to buffer (apply squelch if needed)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        for (unsigned int i = 0; i < num_written; i++) {
            // If squelched, add silence instead of the actual sample
            float sample = is_squelched ? 0.0f : resampled_buffer[i];
            audio_buffer.push_back(sample);
        }
    }
}

// Thread function for RTL-SDR reading
void rtl_thread_function(rtlsdr_dev_t *dev) {
    printf("Starting RTL-SDR thread\n");
    if (rtlsdr_read_async(dev, rtl_callback, nullptr, 0, RTL_READ_SIZE) < 0) {
        std::cerr << "Failed to start async reading\n";
        running = false;
    }
    printf("RTL-SDR thread ending\n");
}

// Function to update Icecast metadata
void update_icecast_metadata(shout_t* shout, double freq_mhz, float signal_db) {
    if (!shout || !icecast_connected.load()) {
        return;
    }
    
    // Format frequency as artist name
    std::stringstream artist_ss;
    artist_ss << std::fixed << std::setprecision(2) << freq_mhz << " MHz";
    std::string artist = artist_ss.str();
    
    // Format signal strength as song title
    std::stringstream title_ss;
    title_ss << std::fixed << std::setprecision(1) << signal_db << " dB";
    std::string title = title_ss.str();
    
    // Format mode
    std::string mode;
    if (current_mode == ModulationMode::AM_MODE) mode = "AM";
    else if (current_mode == ModulationMode::NFM_MODE) mode = "NFM";
    else mode = "WFM";
    
    // Create complete title string with mode
    std::string full_title = title + " [" + mode + "]";
    
    // Try the older API first, as it's more widely supported
    try {
        // Create metadata
        shout_metadata_t *metadata = shout_metadata_new();
        if (!metadata) {
            std::cerr << "Failed to create metadata object" << std::endl;
            return;
        }
        
        // Set artist and title and check return values
        int ret_artist = shout_metadata_add(metadata, "artist", artist.c_str());
        int ret_title = shout_metadata_add(metadata, "title", full_title.c_str());
        
        if (ret_artist != SHOUTERR_SUCCESS || ret_title != SHOUTERR_SUCCESS) {
            std::cerr << "Error adding metadata fields" << std::endl;
            shout_metadata_free(metadata);
            return;
        }
        
        // Try using the older API first
        int result = shout_set_metadata(shout, metadata);
        
        // Free metadata object
        shout_metadata_free(metadata);
        
        if (result != SHOUTERR_SUCCESS) {
            std::cerr << "Error updating metadata: " << shout_get_error(shout) << std::endl;
        } else {
            std::cout << "Updated metadata: " << artist << " - " << full_title << std::endl;
        }
    } catch (...) {
        std::cerr << "Exception in metadata update" << std::endl;
    }
}

// Function to reconnect to Icecast
bool reconnect_icecast(shout_t* &shout) {
    std::cout << "Attempting to reconnect to Icecast...\n";
    
    // Close existing connection if any
    if (shout) {
        shout_close(shout);
        shout_free(shout);
        shout = nullptr;  // Ensure pointer is nullified
    }
    
    // Create new connection
    shout = shout_new();
    if (!shout) {
        std::cerr << "Failed to create new shout instance\n";
        icecast_connected = false;
        return false;
    }
    
    // Configure connection
    shout_set_host(shout, g_config.icecast_host.c_str());
    shout_set_port(shout, g_config.icecast_port);
    shout_set_mount(shout, g_config.icecast_mount.c_str());
    shout_set_user(shout, g_config.icecast_user.c_str());
    shout_set_password(shout, g_config.icecast_password.c_str());
    
    // Use older API calls for better compatibility
    shout_set_format(shout, SHOUT_FORMAT_MP3);
    
    shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP);
    
    // Set station name using older API call
    shout_set_name(shout, g_config.icecast_station_title.c_str());
    
    // Use blocking mode for initial connection
    shout_set_nonblocking(shout, 0);
    
    printf("Connecting to Icecast server %s:%d%s...\n", 
           g_config.icecast_host.c_str(), g_config.icecast_port, g_config.icecast_mount.c_str());
    
    int err = shout_open(shout);
    if (err == SHOUTERR_SUCCESS) {
        std::cout << "Successfully connected to Icecast\n";
        icecast_connected = true;
        
        // Set initial metadata
        uint32_t current_freq = rtlsdr_get_center_freq(g_dev);
        float current_freq_mhz = current_freq / 1e6;
        update_icecast_metadata(shout, current_freq_mhz, signal_strength.load());
        last_metadata_update = std::chrono::steady_clock::now();
        
        return true;
    }
    
    std::cerr << "Failed to connect to Icecast: " << shout_get_error(shout) << std::endl;
    icecast_connected = false;
    return false;
}

// Thread function for Icecast streaming
void icecast_thread_function(shout_t* shout) {
    printf("Starting Icecast streaming thread\n");
    
    int consecutive_errors = 0;
    const std::chrono::milliseconds reconnect_delay(g_config.reconnect_delay_ms);
    auto last_connection_check = std::chrono::steady_clock::now();
    last_metadata_update = std::chrono::steady_clock::now();
    
    while (running) {
        // Periodically check connection status (every 5 seconds)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_connection_check).count() >= 5) {
            if (icecast_connected.load()) {
                // Only check if we think we're connected
                icecast_connected = check_icecast_connection(shout);
            }
            last_connection_check = now;
        }
        
        // Check if it's time to update metadata (every METADATA_UPDATE_INTERVAL_SEC seconds)
        if (icecast_connected.load() && 
            std::chrono::duration_cast<std::chrono::seconds>(now - last_metadata_update).count() >= METADATA_UPDATE_INTERVAL_SEC) {
            
            if (g_dev) {
                uint32_t current_freq = rtlsdr_get_center_freq(g_dev);
                float current_freq_mhz = current_freq / 1e6;
                update_icecast_metadata(shout, current_freq_mhz, signal_strength.load());
            }
            last_metadata_update = now;
        }
        
        // Check connection status
        if (!icecast_connected) {
            std::cout << "Not connected to Icecast, attempting to reconnect...\n";
            
            if (consecutive_errors < g_config.reconnect_attempts) {
                if (reconnect_icecast(shout)) {
                    consecutive_errors = 0;
                } else {
                    consecutive_errors++;
                    std::cout << "Reconnection attempt " << consecutive_errors 
                              << " of " << g_config.reconnect_attempts << " failed\n";
                    std::this_thread::sleep_for(reconnect_delay);
                    continue;
                }
            } else {
                std::cerr << "Failed to reconnect after " << g_config.reconnect_attempts 
                         << " attempts, waiting longer...\n";
                std::this_thread::sleep_for(std::chrono::seconds(30));
                consecutive_errors = 0;
                continue;
            }
        }
        
        // Process MP3 data if connected
        MP3Chunk chunk;
        bool have_data = false;
        
        // Get MP3 data from queue
        {
            std::lock_guard<std::mutex> lock(mp3_buffer_mutex);
            if (!mp3_queue.empty()) {
                chunk = std::move(mp3_queue.front());
                mp3_queue.pop_front();
                have_data = true;
            }
        }
        
        if (have_data) {
            // Verify connection before sending
            if (!check_icecast_connection(shout)) {
                // Put the chunk back in the queue if there's space
                std::lock_guard<std::mutex> lock(mp3_buffer_mutex);
                if (mp3_queue.size() < MAX_MP3_QUEUE_SIZE) {
                    mp3_queue.push_front(std::move(chunk));
                }
                continue;
            }
            
            // Send data
            last_packet_size.store(0);
            int ret = shout_send(shout, chunk.data.data(), chunk.size);
            last_packet_size.store(chunk.size);
            
            if (ret == SHOUTERR_SUCCESS) {
                consecutive_errors = 0;
                // Wait until it's time to send the next chunk
                shout_sync(shout);
            } else {
                std::cerr << "Icecast error: " << shout_get_error(shout) << std::endl;
                icecast_connected = false;
                consecutive_errors++;
                
                // Put the chunk back in the queue if there's space
                std::lock_guard<std::mutex> lock(mp3_buffer_mutex);
                if (mp3_queue.size() < MAX_MP3_QUEUE_SIZE) {
                    mp3_queue.push_front(std::move(chunk));
                }
            }
        } else {
            // No data available, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    printf("Icecast streaming thread ending\n");
}

std::string get_mode_text(ModulationMode mode) {
    if (mode == ModulationMode::AM_MODE) return "AM";
    else if (mode == ModulationMode::NFM_MODE) return "NFM";
    else return "WFM";
}

// Function to print status information
void print_status() {
    // Get audio buffer info
    float buffer_seconds;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        buffer_seconds = static_cast<float>(audio_buffer.size()) / g_config.audio_rate;
    }
    
    // Get MP3 queue info
    size_t queue_size;
    {
        std::lock_guard<std::mutex> lock(mp3_buffer_mutex);
        queue_size = mp3_queue.size();
    }
    
    float signal_db = signal_strength.load();
    size_t packet = last_packet_size.load();
    
    // Get current frequency from the device
    uint32_t current_freq = 0;
    if (g_dev) {
        current_freq = rtlsdr_get_center_freq(g_dev);
    }
    float current_freq_mhz = current_freq / 1e6;
    
    std::string signalBar = std::string(std::max(0, static_cast<int>((signal_db + 30) / 1.9375)), '#') + std::string(16 - std::max(0, static_cast<int>((signal_db + 30) / 1.9375)), ' ');
    
    // Add squelch indicator
    std::string squelchStatus;
    if (g_config.squelch_enabled) {
        squelchStatus = squelch_active ? "MUTED" : "OPEN";
    } else {
        squelchStatus = "OFF";
    }
    
    // Add low-cut filter status
    std::string filterStatus;
    if (g_config.lowcut_enabled) {
        filterStatus = std::to_string(static_cast<int>(g_config.lowcut_freq)) + "Hz";
    } else {
        filterStatus = "OFF";
    }
    
    // Add Icecast connection status with more detail
    std::string connectionStatus;
    if (icecast_connected.load()) {
        connectionStatus = "Connected";
    } else {
        connectionStatus = "Disconnected";
    }

    static int disconnectedCounter=0;
    if (packet > 0) {
        disconnectedCounter=0;
    } else {
        disconnectedCounter++;
    }

    if (disconnectedCounter > 2) 
    {        
        icecast_connected = false;
    }
    
    // Add queue status
    std::string queueStatus = std::to_string(queue_size) + "/" + std::to_string(MAX_MP3_QUEUE_SIZE);

    std::cout << std::fixed << std::setprecision(3)
                <<"[rtl_icecast] "
                << current_freq_mhz << " MHz | "
                << get_mode_text(current_mode) << " | "
                << "Squelch: " << squelchStatus << " | "                
                << "Buffer: " << buffer_seconds << "s | "
                << "Signal: [" << signalBar << "] " << signal_db << " dB | "
                << "mp3-Queue: " << queueStatus << " | "
                << "Last: " << packet << " bytes | "
                << connectionStatus 
                << std::endl;
}

void print_usage() {
    std::cout << "Usage: rtl_icecast [options]\n"
              << "Options:\n"
              << "  -c, --config <file>    Use specified config file (default: config.ini)\n"
              << "  -h, --help             Show this help message\n"
              << std::endl;
}

// Function to change frequency
void change_frequency(double new_freq_mhz) {
    if (!g_dev) return;
    
    uint32_t freq_hz = static_cast<uint32_t>(new_freq_mhz * 1e6);
    if (rtlsdr_set_center_freq(g_dev, freq_hz) < 0) {
        std::cerr << "Failed to set frequency to " << new_freq_mhz << " MHz\n";
    } else {
        g_config.center_freq = new_freq_mhz;
        std::cout << "Tuned to " << new_freq_mhz << " MHz\n";
    }
}

// Function to toggle squelch
void toggle_squelch() {
    g_config.squelch_enabled = !g_config.squelch_enabled;
    std::cout << "Squelch " << (g_config.squelch_enabled ? "enabled" : "disabled") 
              << " (threshold: " << g_config.squelch_threshold << " dB)" << std::endl;
}

// Function to set squelch threshold
void set_squelch_threshold(float threshold) {
    g_config.squelch_threshold = threshold;
    std::cout << "Squelch threshold set to " << threshold << " dB" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file = "config.ini";
    bool force_narrow = false;
    bool force_squelch = false;
    float squelch_level = -30.0f;
    bool force_lowcut = false;
    float lowcut_freq = 300.0f;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: Config file path not specified\n";
                return 1;
            }
        } 
    }

    // Load configuration
    try {
        g_config = ConfigParser::parse_config(config_file);
        if (force_narrow) {
            g_config.wide_fm = false;
        }
        if (force_squelch) {
            g_config.squelch_enabled = true;
            g_config.squelch_threshold = squelch_level;
        }
        if (force_lowcut) {
            g_config.lowcut_enabled = true;
            g_config.lowcut_freq = lowcut_freq;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return 1;
    }

    printf("scanlist size %ld\n", g_config.scanlist.size());
    for (std::size_t ind = 0; ind < g_config.scanlist.size(); ind++ ) {
        printf("Frequency %f,  channel name %s\n", g_config.scanlist[ind].frequency, g_config.scanlist[ind].ch_name.c_str());
    }

    scanner = new Scanner(g_config.scanlist);

    // Initialize squelch state
    last_signal_above_threshold = std::chrono::steady_clock::now();
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, signal_handler);
    
    last_stats_time = std::chrono::steady_clock::now();
    
    // Initialize modulation
    init_modulation(g_config.mode);
    
    // Initialize low-cut filter
    init_lowcut_filter();
    
    // Initialize RTL-SDR
    if (rtlsdr_open(&g_dev, 0) < 0) {
        std::cerr << "Failed to open RTL-SDR device\n";
        return 1;
    }
    
    // Convert MHz to Hz for the RTL-SDR API
    uint32_t freq_hz = static_cast<uint32_t>(g_config.center_freq * 1e6);
    
    // Apply PPM correction if specified
    if (g_config.ppm_correction != 0) {
        printf("Setting frequency correction to %d ppm\n", g_config.ppm_correction);
        rtlsdr_set_freq_correction(g_dev, g_config.ppm_correction);
    }
    
    rtlsdr_set_center_freq(g_dev, freq_hz); 
    rtlsdr_set_sample_rate(g_dev, g_config.sample_rate);
    rtlsdr_set_tuner_gain_mode(g_dev, g_config.gain_mode);
    if (g_config.gain_mode == 1) {
        
        int tuner_gains[16];
        int num_gains = rtlsdr_get_tuner_gains(g_dev, tuner_gains);
        printf("Allowed gain setting values (1/10 dB): ");
        for (int i = 0; i < num_gains; i++) {
            printf("%d ", tuner_gains[i]);
        }
        printf("\n");   
        
        // Set the specified gain or use a default if not specified
        int gain_to_use = g_config.tuner_gain;
        if (gain_to_use == 0) {
            // Default to 9.0 dB (90 tenths of a dB) if not specified
            gain_to_use = 90;
            printf("No specific gain value provided, using default: %d.%d dB\n", gain_to_use/10, gain_to_use%10);
        } else {
            printf("Setting tuner gain to %d.%d dB\n", gain_to_use/10, gain_to_use%10);
        }
        
        rtlsdr_set_tuner_gain(g_dev, gain_to_use);
    }
    rtlsdr_reset_buffer(g_dev);
    
    // Initialize resampler
    float resamp_ratio = (float)g_config.audio_rate / g_config.sample_rate * 1.00f;  
    resampler = msresamp_rrrf_create(resamp_ratio, 60.0f);
    
    // Initialize LAME
    lame_t lame = lame_init();
    lame_set_in_samplerate(lame, g_config.audio_rate);
    lame_set_out_samplerate(lame, g_config.audio_rate);
    lame_set_num_channels(lame, 1);
    lame_set_mode(lame, MONO);
    lame_set_quality(lame, g_config.mp3_quality);
    lame_set_brate(lame, g_config.mp3_bitrate);
    lame_set_VBR(lame, vbr_off);
    if (lame_init_params(lame) < 0) {
        std::cerr << "Failed to initialize LAME\n";
        return 1;
    }
    
    // Initialize Icecast
    shout_init();
    shout_t *shout = shout_new();
    if (!shout) {
        std::cerr << "Could not allocate shout_t\n";
        return 1;
    }

    shout_set_host(shout, g_config.icecast_host.c_str());
    shout_set_port(shout, g_config.icecast_port);
    shout_set_mount(shout, g_config.icecast_mount.c_str());
    shout_set_user(shout, g_config.icecast_user.c_str());
    shout_set_password(shout, g_config.icecast_password.c_str());
    
    // Use older API calls for better compatibility
    shout_set_format(shout, SHOUT_FORMAT_MP3);
    
    shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP);
    
    // Set station name using older API call
    shout_set_name(shout, g_config.icecast_station_title.c_str());
    
    // Use blocking mode for initial connection
    shout_set_nonblocking(shout, 0);
    
    printf("Connecting to Icecast server %s:%d%s...\n", 
           g_config.icecast_host.c_str(), g_config.icecast_port, g_config.icecast_mount.c_str());
    
    int err = shout_open(shout);
    if (err == SHOUTERR_SUCCESS) {
        printf("Connected to Icecast server\n");
        icecast_connected = true;
        
        // Set initial metadata
        last_metadata_update = std::chrono::steady_clock::now();
    } else {
        std::cerr << "Error connecting to Icecast: " << shout_get_error(shout) << std::endl;
        std::cerr << "Will attempt to reconnect in the streaming thread\n";
        icecast_connected = false;
        // Continue anyway - the streaming thread will handle reconnection
    }
    
    // Start RTL-SDR thread
    std::thread rtl_thread(rtl_thread_function, g_dev);
    
    // Start Icecast streaming thread
    std::thread icecast_thread(icecast_thread_function, shout);

    // Buffers for processing
    std::vector<short> pcm_buffer(CHUNK_SIZE);
    std::vector<unsigned char> mp3_buffer(MP3_BUFFER_SIZE);
    
    auto last_status_time = std::chrono::steady_clock::now();
    
    // pre-buffer
    printf("Pre-buffering...\n");
    while (true) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        if (audio_buffer.size() >= CHUNK_SIZE*2) {
            break; // We have enough samples, exit the loop
        }
        // Optionally, you can add a small sleep to avoid busy waiting        
    }
    
    printf("All ready - Let's go!\n");
    
    while (running) 
    {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status_time).count() >= 1) {
            print_status();
            last_status_time = now;
        }

        // Check if we have enough samples
        bool have_chunk = false;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            have_chunk = audio_buffer.size() >= CHUNK_SIZE;
        }
        
        if (have_chunk) {
            // Extract chunk and convert to PCM            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    float sample = std::max(-1.0f, std::min(1.0f, audio_buffer.front()));
                    sample *= 0.7f; // Prevent clipping
                    pcm_buffer[i] = static_cast<short>(sample * 32767.0f);
                    audio_buffer.pop_front();
                }
            }
            
            // Encode to MP3
            int mp3_size = lame_encode_buffer(lame,
                                            pcm_buffer.data(),
                                            nullptr,
                                            CHUNK_SIZE,
                                            mp3_buffer.data(),
                                            mp3_buffer.size());
            
            if (mp3_size > 0) {
                // Add MP3 data to queue
                std::lock_guard<std::mutex> lock(mp3_buffer_mutex);
                if (mp3_queue.size() < MAX_MP3_QUEUE_SIZE) {
                    MP3Chunk chunk;
                    chunk.data.assign(mp3_buffer.begin(), mp3_buffer.begin() + mp3_size);
                    chunk.size = mp3_size;
                    mp3_queue.push_back(std::move(chunk));
                } else {
                    std::cerr << "MP3 queue full, dropping chunk\n";
                }
            }
        } else {
            // Add a small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (g_config.scanEnabled) {
            double frq = scanner->NextCh(squelch_active);
            if (frq != 0) {
                change_frequency(frq);
            }
        }
    }
    
    // Cleanup
    rtlsdr_cancel_async(g_dev);  // Stop async reading
    if (rtl_thread.joinable()) {
        rtl_thread.join();
    }
    if (icecast_thread.joinable()) {
        icecast_thread.join();
    }
    msresamp_rrrf_destroy(resampler);
    iirfilt_crcf_destroy(filter);  // Clean up filter
    
    // Clean up low-cut filter
    if (lowcut_filter) {
        iirfilt_rrrf_destroy(lowcut_filter);
    }
    
    delete scanner;
    rtlsdr_close(g_dev);
    lame_close(lame);
    shout_close(shout);
    shout_free(shout);
    shout_shutdown();
    
    return 0;
}
