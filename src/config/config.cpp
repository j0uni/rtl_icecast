#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include "../radio/scanner.h"

Config::Config() :
    sample_rate(1024000),
    center_freq(99.9),  // 99.9 MHz
    gain_mode(0),
    tuner_gain(0),
    ppm_correction(0),
    mode(ModulationMode::WFM_MODE),
    scanEnabled(false),
    step_delay_ms(100),
    audio_rate(48000),
    mp3_bitrate(128),
    mp3_quality(2),
    audio_buffer_seconds(2),
    squelch_enabled(false),
    squelch_threshold(-30.0f),
    squelch_hold_time(500),
    lowcut_enabled(false),
    lowcut_freq(300.0f),    // 300 Hz default cutoff
    lowcut_order(4),        // 4th order filter by default
    icecast_host("localhost"),  // Changed from server.com to localhost
    icecast_port(8000),
    icecast_mount("/stream"),   // Changed from /streamers_mount to /stream
    icecast_password("hackme"), // Changed from streamers_password to hackme (common default)
    icecast_user("source"),     // icecast wants "source"
    icecast_protocol("http"),
    icecast_format("mp3"),
    reconnect_attempts(5),
    reconnect_delay_ms(2000),
    icecast_station_title("RTL-SDR Radio")
{}

bool Config::loadFromFile(const std::string& filename) {
    try {
        // Keep this function but don't output debug info
        // We'll keep the function in case it's called elsewhere
        
        // Check if file exists and is readable
        std::ifstream file(filename);
        if (!file.good()) {
            std::cerr << "[ERROR] Configuration file not found or not readable: " << filename << std::endl;
            return false;
        }
        
        *this = ConfigParser::parse_config(filename);
        
        // Keep this function but don't output debug info
        // We'll keep the function in case it's called elsewhere
        
        validate();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Error loading configuration: " << e.what() << std::endl;
        return false;
    }
}

void Config::validate() {
    // Validate sample rate
    if (sample_rate <= 0) {
        throw ConfigException("Sample rate must be positive");
    }
    
    // Validate center frequency
    if (center_freq <= 0) {
        throw ConfigException("Center frequency must be positive");
    }
    
    // Validate audio rate
    if (audio_rate <= 0) {
        throw ConfigException("Audio rate must be positive");
    }
    
    // Validate MP3 bitrate
    if (mp3_bitrate <= 0) {
        throw ConfigException("MP3 bitrate must be positive");
    }
    
    // Validate MP3 quality
    if (mp3_quality < 0 || mp3_quality > 9) {
        throw ConfigException("MP3 quality must be between 0 and 9");
    }
    
    // Validate audio buffer seconds
    if (audio_buffer_seconds <= 0) {
        throw ConfigException("Audio buffer seconds must be positive");
    }
    
    // Validate low-cut frequency
    if (lowcut_freq <= 0) {
        throw ConfigException("Low-cut frequency must be positive");
    }
    
    // Validate low-cut order
    if (lowcut_order <= 0) {
        throw ConfigException("Low-cut order must be positive");
    }
    
    // Validate Icecast port
    if (icecast_port <= 0 || icecast_port > 65535) {
        throw ConfigException("Icecast port must be between 1 and 65535");
    }
    
    // Validate reconnect attempts
    if (reconnect_attempts < 0) {
        throw ConfigException("Reconnect attempts must be non-negative");
    }
    
    // Validate reconnect delay
    if (reconnect_delay_ms < 0) {
        throw ConfigException("Reconnect delay must be non-negative");
    }
}

std::string Config::getModeString() const {
    switch (mode) {
        case ModulationMode::AM_MODE:
            return "AM";
        case ModulationMode::NFM_MODE:
            return "NFM";
        case ModulationMode::WFM_MODE:
            return "WFM";
        default:
            return "Unknown";
    }
}

ModulationMode Config::stringToMode(const std::string& mode_str) {
    std::string lower_mode = mode_str;
    std::transform(lower_mode.begin(), lower_mode.end(), lower_mode.begin(), ::tolower);
    
    if (lower_mode == "am") {
        return ModulationMode::AM_MODE;
    } else if (lower_mode == "narrow" || lower_mode == "nfm") {
        return ModulationMode::NFM_MODE;
    } else {
        return ModulationMode::WFM_MODE;
    }
}

namespace ConfigParser {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

std::string remove_comment(const std::string& str) {
    size_t pos = str.find_first_of("#;");
    if (pos == std::string::npos) return str;
    return str.substr(0, pos);
}

std::map<std::string, std::map<std::string, std::string>> parse_ini(const std::string& filename) {
    std::map<std::string, std::map<std::string, std::string>> result;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw ConfigException("Could not open config file: " + filename);
    }
    
    std::string section;
    std::string line;
    
    while (std::getline(file, line)) {
        line = trim(remove_comment(line));
        
        if (line.empty()) {
            continue;
        }
        
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            section = line.substr(1, line.length() - 2);
            continue;
        }
        
        auto delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiter_pos));
            std::string value = trim(line.substr(delimiter_pos + 1));
            
            result[section][key] = value;
        }
    }
    
    return result;
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> substrings;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        substrings.push_back(item);
    }

    return substrings;
}

// Implementation of parse_config function
Config parse_config(const std::string &filename) {
    Config config;
    auto ini_data = parse_ini(filename);
    
    // Parse RTL-SDR section
    if (ini_data.count("rtl_sdr")) {
        auto& section = ini_data["rtl_sdr"];
        
        if (section.count("sample_rate")) {
            config.setSampleRate(std::stoi(section["sample_rate"]));
        }
        
        if (section.count("center_freq_mhz")) {
            config.setCenterFreq(std::stod(section["center_freq_mhz"]));
        }
        
        if (section.count("gain_mode")) {
            config.setGainMode(std::stoi(section["gain_mode"]));
        }
        
        if (section.count("tuner_gain")) {
            config.setTunerGain(std::stoi(section["tuner_gain"]));
        }
        
        if (section.count("ppm_correction")) {
            config.setPpmCorrection(std::stoi(section["ppm_correction"]));
        }
        
        if (section.count("fm_mode")) {
            std::string mode = section["fm_mode"];
            config.setMode(Config::stringToMode(mode));
        }
    }
    
    // Parse scanner section
    if (ini_data.count("scanner")) {
        auto& section = ini_data["scanner"];
        
        if (section.count("enabled")) {
            std::string enabled = section["enabled"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.setScanEnabled(enabled == "true" || enabled == "1");
        }
        
        if (section.count("step_delay_ms")) {
            config.setStepDelayMs(std::stoi(section["step_delay_ms"]));
        }
    }
    
    // Parse scanlist section
    if (ini_data.count("scanlist")) {
        auto& section = ini_data["scanlist"];
        std::vector<ScanList> scanlist;
        
        for (const auto& entry : section) {
            std::string name = entry.first;
            std::string freqs_str = entry.second;
            
            std::vector<double> frequencies;
            auto freq_strings = splitString(freqs_str, ',');
            for (const auto& freq_str : freq_strings) {
                try {
                    double freq = std::stod(trim(freq_str));
                    frequencies.push_back(freq);
                } catch (const std::exception& e) {
                    // Skip invalid frequencies
                }
            }
            
            if (!frequencies.empty()) {
                scanlist.push_back({name, frequencies});
            }
        }
        
        config.setScanlist(scanlist);
    }
    
    // Parse audio section
    if (ini_data.count("audio")) {
        auto& section = ini_data["audio"];
        
        if (section.count("audio_rate")) {
            config.setAudioRate(std::stoi(section["audio_rate"]));
        }
        
        if (section.count("mp3_bitrate")) {
            config.setMp3Bitrate(std::stoi(section["mp3_bitrate"]));
        }
        
        if (section.count("mp3_quality")) {
            config.setMp3Quality(std::stoi(section["mp3_quality"]));
        }
        
        if (section.count("audio_buffer_seconds")) {
            config.setAudioBufferSeconds(std::stoi(section["audio_buffer_seconds"]));
        }
    }
    
    // Parse audio_filters section
    if (ini_data.count("audio_filters")) {
        auto& section = ini_data["audio_filters"];
        
        if (section.count("lowcut_enabled")) {
            std::string enabled = section["lowcut_enabled"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.setLowcutEnabled(enabled == "true" || enabled == "1");
        }
        
        if (section.count("lowcut_freq")) {
            config.setLowcutFreq(std::stof(section["lowcut_freq"]));
        }
        
        if (section.count("lowcut_order")) {
            config.setLowcutOrder(std::stoi(section["lowcut_order"]));
        }
    }
    
    // Parse squelch section
    if (ini_data.count("squelch")) {
        auto& section = ini_data["squelch"];
        
        if (section.count("enabled")) {
            std::string enabled = section["enabled"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.setSquelchEnabled(enabled == "true" || enabled == "1");
        }
        
        if (section.count("threshold")) {
            config.setSquelchThreshold(std::stof(section["threshold"]));
        }
        
        if (section.count("hold_time")) {
            config.setSquelchHoldTime(std::stoi(section["hold_time"]));
        }
    }
    
    // Parse icecast section
    if (ini_data.count("icecast")) {
        auto& section = ini_data["icecast"];
        
        if (section.count("host")) {
            config.setIcecastHost(section["host"]);
        }
        
        if (section.count("port")) {
            config.setIcecastPort(std::stoi(section["port"]));
        }
        
        if (section.count("mount")) {
            config.setIcecastMount(section["mount"]);
        }
        
        if (section.count("user")) {
            config.setIcecastUser(section["user"]);
        }
        
        if (section.count("password")) {
            config.setIcecastPassword(section["password"]);
        }
        
        if (section.count("protocol")) {
            config.setIcecastProtocol(section["protocol"]);
        }
        
        if (section.count("format")) {
            config.setIcecastFormat(section["format"]);
        }
        
        if (section.count("station_title")) {
            config.setIcecastStationTitle(section["station_title"]);
        }
        
        if (section.count("reconnect_attempts")) {
            config.setReconnectAttempts(std::stoi(section["reconnect_attempts"]));
        }
        
        if (section.count("reconnect_delay_ms")) {
            config.setReconnectDelayMs(std::stoi(section["reconnect_delay_ms"]));
        }
    }
    
    return config;
}

} // namespace ConfigParser
