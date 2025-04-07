#pragma once

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "../radio/scanner.h"  // Include for ScanList definition

enum class ModulationMode {
    AM_MODE,
    NFM_MODE,
    WFM_MODE
};

class ConfigException : public std::runtime_error {
public:
    ConfigException(const std::string& message) : std::runtime_error(message) {}
};

// Forward declaration
class Config;

// ConfigParser namespace declaration
namespace ConfigParser {
    // Parse configuration from file
    Config parse_config(const std::string &filename);
    
    // Helper function declarations
    std::string trim(const std::string& str);
    std::string remove_comment(const std::string& str);
    std::map<std::string, std::map<std::string, std::string>> parse_ini(const std::string& filename);
}

class Config {
private:
    // RTL-SDR settings
    int sample_rate;
    double center_freq;  // in MHz
    int gain_mode;
    int tuner_gain;      // in tenths of dB, only used when gain_mode = 1
    int ppm_correction;  // frequency correction in PPM
    ModulationMode mode;

    // Scanner settings
    bool scanEnabled;
    uint16_t step_delay_ms;

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

    // Station title setting
    std::string icecast_station_title;

public:
    // Constructor with default values
    Config();
    
    // Load configuration from file
    bool loadFromFile(const std::string& filename);
    
    // Validate configuration
    void validate();
    
    // Get modulation mode as string
    std::string getModeString() const;
    
    // Convert string to modulation mode
    static ModulationMode stringToMode(const std::string& mode_str);
    
    // Getters for RTL-SDR settings
    int getSampleRate() const { return sample_rate; }
    double getCenterFreq() const { return center_freq; }
    int getGainMode() const { return gain_mode; }
    int getTunerGain() const { return tuner_gain; }
    int getPpmCorrection() const { return ppm_correction; }
    ModulationMode getMode() const { return mode; }
    
    // Setters for RTL-SDR settings
    void setSampleRate(int rate) { sample_rate = rate; }
    void setCenterFreq(double freq) { center_freq = freq; }
    void setGainMode(int mode) { gain_mode = mode; }
    void setTunerGain(int gain) { tuner_gain = gain; }
    void setPpmCorrection(int ppm) { ppm_correction = ppm; }
    void setMode(ModulationMode m) { mode = m; }
    
    // Getters for Scanner settings
    bool isScanEnabled() const { return scanEnabled; }
    uint16_t getStepDelayMs() const { return step_delay_ms; }
    
    // Setters for Scanner settings
    void setScanEnabled(bool enabled) { scanEnabled = enabled; }
    void setStepDelayMs(uint16_t delay) { step_delay_ms = delay; }
    
    // Getter for Scanlist
    const std::vector<ScanList>& getScanlist() const { return scanlist; }
    
    // Setter for Scanlist
    void setScanlist(const std::vector<ScanList>& list) { scanlist = list; }
    
    // Getters for Audio settings
    int getAudioRate() const { return audio_rate; }
    int getMp3Bitrate() const { return mp3_bitrate; }
    int getMp3Quality() const { return mp3_quality; }
    int getAudioBufferSeconds() const { return audio_buffer_seconds; }
    
    // Setters for Audio settings
    void setAudioRate(int rate) { audio_rate = rate; }
    void setMp3Bitrate(int bitrate) { mp3_bitrate = bitrate; }
    void setMp3Quality(int quality) { mp3_quality = quality; }
    void setAudioBufferSeconds(int seconds) { audio_buffer_seconds = seconds; }
    
    // Getters for Squelch settings
    bool isSquelchEnabled() const { return squelch_enabled; }
    float getSquelchThreshold() const { return squelch_threshold; }
    int getSquelchHoldTime() const { return squelch_hold_time; }
    
    // Setters for Squelch settings
    void setSquelchEnabled(bool enabled) { squelch_enabled = enabled; }
    void setSquelchThreshold(float threshold) { squelch_threshold = threshold; }
    void setSquelchHoldTime(int time) { squelch_hold_time = time; }
    
    // Getters for Audio filter settings
    bool isLowcutEnabled() const { return lowcut_enabled; }
    float getLowcutFreq() const { return lowcut_freq; }
    int getLowcutOrder() const { return lowcut_order; }
    
    // Setters for Audio filter settings
    void setLowcutEnabled(bool enabled) { lowcut_enabled = enabled; }
    void setLowcutFreq(float freq) { lowcut_freq = freq; }
    void setLowcutOrder(int order) { lowcut_order = order; }
    
    // Getters for Icecast settings
    const std::string& getIcecastHost() const { return icecast_host; }
    int getIcecastPort() const { return icecast_port; }
    const std::string& getIcecastMount() const { return icecast_mount; }
    const std::string& getIcecastPassword() const { return icecast_password; }
    const std::string& getIcecastUser() const { return icecast_user; }
    const std::string& getIcecastProtocol() const { return icecast_protocol; }
    const std::string& getIcecastFormat() const { return icecast_format; }
    int getReconnectAttempts() const { return reconnect_attempts; }
    int getReconnectDelayMs() const { return reconnect_delay_ms; }
    
    // Setters for Icecast settings
    void setIcecastHost(const std::string& host) { icecast_host = host; }
    void setIcecastPort(int port) { icecast_port = port; }
    void setIcecastMount(const std::string& mount) { icecast_mount = mount; }
    void setIcecastPassword(const std::string& password) { icecast_password = password; }
    void setIcecastUser(const std::string& user) { icecast_user = user; }
    void setIcecastProtocol(const std::string& protocol) { icecast_protocol = protocol; }
    void setIcecastFormat(const std::string& format) { icecast_format = format; }
    void setReconnectAttempts(int attempts) { reconnect_attempts = attempts; }
    void setReconnectDelayMs(int delay) { reconnect_delay_ms = delay; }
    
    // Getter for Station title setting
    const std::string& getIcecastStationTitle() const { return icecast_station_title; }
    
    // Setter for Station title setting
    void setIcecastStationTitle(const std::string& title) { icecast_station_title = title; }
    
    // Make ConfigParser::parse_config a friend function
    friend Config ConfigParser::parse_config(const std::string &filename);
};
