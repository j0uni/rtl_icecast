#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

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
        throw std::runtime_error("Could not open config file: " + filename);
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
            config.sample_rate = std::stoi(section["sample_rate"]);
        }
        
        if (section.count("center_freq_mhz")) {
            config.center_freq = std::stod(section["center_freq_mhz"]);
        }
        
        if (section.count("gain_mode")) {
            config.gain_mode = std::stoi(section["gain_mode"]);
        }
        
        if (section.count("tuner_gain")) {
            config.tuner_gain = std::stoi(section["tuner_gain"]);
        }
        
        if (section.count("ppm_correction")) {
            config.ppm_correction = std::stoi(section["ppm_correction"]);
        }
        
        if (section.count("fm_mode")) {
            std::string mode = section["fm_mode"];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            config.wide_fm = (mode == "wide");
            if (mode == "am") config.mode = ModulationMode::AM_MODE;
            else if (mode == "narrow") config.mode = ModulationMode::NFM_MODE;
            else config.mode = ModulationMode::WFM_MODE;
        }
    }
    
    // Parse audio section
    if (ini_data.count("audio")) {
        auto& section = ini_data["audio"];
        
        if (section.count("audio_rate")) {
            config.audio_rate = std::stoi(section["audio_rate"]);
        }
        
        if (section.count("mp3_bitrate")) {
            config.mp3_bitrate = std::stoi(section["mp3_bitrate"]);
        }
        
        if (section.count("mp3_quality")) {
            config.mp3_quality = std::stoi(section["mp3_quality"]);
        }
        
        if (section.count("audio_buffer_seconds")) {
            config.audio_buffer_seconds = std::stoi(section["audio_buffer_seconds"]);
        }
    }
    
    // Parse audio_filters section
    if (ini_data.count("audio_filters")) {
        auto& section = ini_data["audio_filters"];
        
        if (section.count("lowcut_enabled")) {
            std::string enabled = section["lowcut_enabled"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.lowcut_enabled = (enabled == "true" || enabled == "1");
        }
        
        if (section.count("lowcut_freq")) {
            config.lowcut_freq = std::stof(section["lowcut_freq"]);
        }
        
        if (section.count("lowcut_order")) {
            config.lowcut_order = std::stoi(section["lowcut_order"]);
        }
    }
    
    // Parse squelch section
    if (ini_data.count("squelch")) {
        auto& section = ini_data["squelch"];
        
        if (section.count("enabled")) {
            std::string enabled = section["enabled"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.squelch_enabled = (enabled == "true" || enabled == "1");
        }
        
        if (section.count("threshold")) {
            config.squelch_threshold = std::stof(section["threshold"]);
        }
        
        if (section.count("hold_time")) {
            config.squelch_hold_time = std::stoi(section["hold_time"]);
        }
    }
    
    // Parse icecast section
    if (ini_data.count("icecast")) {
        auto& section = ini_data["icecast"];
        
        if (section.count("host")) {
            config.icecast_host = section["host"];
        }
        
        if (section.count("port")) {
            config.icecast_port = std::stoi(section["port"]);
        }
        
        if (section.count("mount")) {
            config.icecast_mount = section["mount"];
        }
        
        if (section.count("user")) {
            config.icecast_user = section["user"];
        }
        
        if (section.count("password")) {
            config.icecast_password = section["password"];
        }
        
        if (section.count("protocol")) {
            config.icecast_protocol = section["protocol"];
        }
        
        if (section.count("format")) {
            config.icecast_format = section["format"];
        }
        
        if (section.count("station_title")) {
            config.icecast_station_title = section["station_title"];
        }
        
        if (section.count("reconnect_attempts")) {
            config.reconnect_attempts = std::stoi(section["reconnect_attempts"]);
        }
        
        if (section.count("reconnect_delay_ms")) {
            config.reconnect_delay_ms = std::stoi(section["reconnect_delay_ms"]);
        }
    }

    // Parse scan section
    if (ini_data.count("scanner")) {
        auto& section = ini_data["scanner"];

        if (section.count("scan")) {
            std::string enabled = section["scan"];
            std::transform(enabled.begin(), enabled.end(), enabled.begin(), ::tolower);
            config.squelch_enabled = (enabled == "true" || enabled == "1");
        }

        if (section.count("step_delay_ms")) {
            config.step_delay_ms = std::stoi(section["step_delay"]);
        }
    }

    // Parse scanlist section
    if (ini_data.count("scanlist")) {
        char delimiter = ',';
        auto& section = ini_data["scanlist"];

        for (std::map<std::string, std::string>::iterator it = section.begin(); it != section.end(); it++) {
            std::vector<std::string> result = splitString(it->second, delimiter);

            ScanList Channel;
            Channel.frequency = std::stod(result[0]);
            Channel.modulation_mode = result[1];
            Channel.ch_name = result[2];
            config.scanlist.push_back(Channel);
        }
    }

    return config;
}

} // namespace ConfigParser 