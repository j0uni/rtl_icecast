#include "config.h"
#include <algorithm>
#include <cctype>

std::string ConfigParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

std::string ConfigParser::remove_comment(const std::string& str) {
    size_t pos = str.find(';');
    if (pos != std::string::npos) {
        return trim(str.substr(0, pos));
    }
    return trim(str);
}

std::map<std::string, std::map<std::string, std::string>> ConfigParser::parse_ini(const std::string& filename) {
    std::map<std::string, std::map<std::string, std::string>> data;
    std::ifstream file(filename);
    std::string section;
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return data;
    }

    while (std::getline(file, line)) {
        line = remove_comment(line);
        if (line.empty()) continue;

        if (line[0] == '[' && line[line.length()-1] == ']') {
            section = line.substr(1, line.length()-2);
            continue;
        }

        size_t pos = line.find('=');
        if (pos != std::string::npos && !section.empty()) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            data[section][key] = value;
        }
    }

    return data;
}

Config ConfigParser::parse_config(const std::string& filename) {
    Config config;
    auto data = parse_ini(filename);

    try {
        // RTL-SDR settings
        if (data.count("rtl_sdr")) {
            const auto& rtl = data["rtl_sdr"];
            if (rtl.count("sample_rate")) config.sample_rate = std::stoi(rtl.at("sample_rate"));
            if (rtl.count("center_freq_mhz")) config.center_freq = std::stod(rtl.at("center_freq_mhz"));
            if (rtl.count("gain_mode")) config.gain_mode = std::stoi(rtl.at("gain_mode"));
            if (rtl.count("fm_mode")) {
                std::string mode = rtl.at("fm_mode");
                std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
                config.wide_fm = (mode == "wide");
            }
        }

        // Audio settings
        if (data.count("audio")) {
            const auto& audio = data["audio"];
            if (audio.count("audio_rate")) config.audio_rate = std::stoi(audio.at("audio_rate"));
            if (audio.count("mp3_bitrate")) config.mp3_bitrate = std::stoi(audio.at("mp3_bitrate"));
            if (audio.count("mp3_quality")) config.mp3_quality = std::stoi(audio.at("mp3_quality"));
            if (audio.count("audio_buffer_seconds")) 
                config.audio_buffer_seconds = std::stoi(audio.at("audio_buffer_seconds"));
        }
        
        // Squelch settings
        if (data.count("squelch")) {
            const auto& squelch = data["squelch"];
            if (squelch.count("enabled")) {
                std::string enabled = squelch.at("enabled");
                std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
                config.squelch_enabled = (enabled == "true" || enabled == "yes" || enabled == "1");
            }
            if (squelch.count("threshold")) config.squelch_threshold = std::stof(squelch.at("threshold"));
            if (squelch.count("hold_time")) config.squelch_hold_time = std::stoi(squelch.at("hold_time"));
        }
        
        // Audio filter settings
        if (data.count("audio_filters")) {
            const auto& filters = data["audio_filters"];
            if (filters.count("lowcut_enabled")) {
                std::string enabled = filters.at("lowcut_enabled");
                std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
                config.lowcut_enabled = (enabled == "true" || enabled == "yes" || enabled == "1");
            }
            if (filters.count("lowcut_freq")) config.lowcut_freq = std::stof(filters.at("lowcut_freq"));
            if (filters.count("lowcut_order")) config.lowcut_order = std::stoi(filters.at("lowcut_order"));
        }

        // Icecast settings
        if (data.count("icecast")) {
            const auto& ice = data["icecast"];
            if (ice.count("host")) config.icecast_host = ice.at("host");
            if (ice.count("port")) config.icecast_port = std::stoi(ice.at("port"));
            if (ice.count("mount")) config.icecast_mount = ice.at("mount");
            if (ice.count("password")) config.icecast_password = ice.at("password");
            if (ice.count("user")) config.icecast_user = ice.at("user");
            if (ice.count("protocol")) config.icecast_protocol = ice.at("protocol");
            if (ice.count("format")) config.icecast_format = ice.at("format");
            if (ice.count("reconnect_attempts")) 
                config.reconnect_attempts = std::stoi(ice.at("reconnect_attempts"));
            if (ice.count("reconnect_delay_ms")) 
                config.reconnect_delay_ms = std::stoi(ice.at("reconnect_delay_ms"));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        std::cerr << "Using default values for failed parameters" << std::endl;
    }

    return config;
} 