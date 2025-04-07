#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>

struct StatusInfo {
    float buffer_seconds{0};
    float signal_level_db{-120.0f};
    size_t packet_size{0};
    bool connected{false};
    double frequency{0.0};
    std::string mode_text;
    bool squelch_active{false};
    bool lowcut_enabled{false};
    bool scanning_active{false};
    std::string scanner_name;
};

class StatusDisplay {
private:
    mutable std::mutex status_mutex;
    StatusInfo current_status;
    bool quiet_mode;
    std::chrono::steady_clock::time_point last_update_time;
    int update_interval_ms;
    
    // Callback for custom display
    std::function<void(const StatusInfo&)> custom_display_callback;
    
public:
    StatusDisplay(bool quiet = false, int update_interval = 1000);
    
    // Update status information
    void updateBufferStatus(float buffer_seconds);
    void updateSignalLevel(float signal_db);
    void updatePacketSize(size_t size);
    void updateConnectionStatus(bool connected);
    void updateFrequency(double freq);
    void updateMode(const std::string& mode);
    void updateSquelchStatus(bool active);
    void updateLowcutStatus(bool enabled);
    void updateScannerStatus(bool active, const std::string& name = "");
    
    // Get current status
    StatusInfo getStatus() const;
    
    // Display status
    void display();
    
    // Set quiet mode
    void setQuietMode(bool quiet);
    
    // Set update interval
    void setUpdateInterval(int interval_ms);
    
    // Set custom display callback
    void setCustomDisplayCallback(std::function<void(const StatusInfo&)> callback);
};
