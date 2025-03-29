#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "scanner.h"

enum class ModulationMode {
    AM_MODE,
    NFM_MODE,
    WFM_MODE
};

struct Config {
    // RTL-SDR settings
    int sample_rate;
    double center_freq;  // in MHz
    int gain_mode;
    bool wide_fm;
    ModulationMode mode;

    // Scanner settings
    bool scan;

    // Scanlist
    std::vector<ScanList> scanlist;

    // Audio settings
    int audio_rate;
    int mp3_bitrate;
    int mp3_quality;
    int audio_buffer_seconds;
    
    // Squelch settings
    bool squelch_enabled;
    float squelch_threshold;    // in dB
    int squelch_hold_time;      // in milliseconds
    
    // Audio filter settings
    bool lowcut_enabled;        // Enable low-cut filter
    float lowcut_freq;          // Low-cut frequency in Hz
    int lowcut_order;           // Filter order (steepness)

    // Icecast settings
    std::string icecast_host;
    int icecast_port;
    std::string icecast_mount;
    std::string icecast_password;
    std::string icecast_user;
    std::string icecast_protocol;
    std::string icecast_format;
    int reconnect_attempts;
    int reconnect_delay_ms;

    // New station title setting
    std::string icecast_station_title;

    // Constructor with default values
    Config() :
        sample_rate(1024000),
        center_freq(99.9),  // 99.9 MHz
        gain_mode(0),
        wide_fm(true),
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
        icecast_host("server.com"),
        icecast_port(8000),
        icecast_mount("/streamers_mount"),
        icecast_password("streamers_password"),
        icecast_user("source"), // icecast wants "source"
        icecast_protocol("http"),
        icecast_format("mp3"),
        reconnect_attempts(5),
        reconnect_delay_ms(2000),
        icecast_station_title("RTL-SDR Radio")
    {}
};

namespace ConfigParser {
    // Helper function declarations
    std::string trim(const std::string& str);
    std::string remove_comment(const std::string& str);
    std::map<std::string, std::map<std::string, std::string>> parse_ini(const std::string& filename);
    
    // Only declare the function, don't define it
    Config parse_config(const std::string &filename);
    
} 