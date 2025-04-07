#include "status_display.h"
#include <iostream>
#include <iomanip>
#include <sstream>

StatusDisplay::StatusDisplay(bool quiet, int update_interval)
    : quiet_mode(quiet), update_interval_ms(update_interval) {
    last_update_time = std::chrono::steady_clock::now();
}

void StatusDisplay::updateBufferStatus(float buffer_seconds) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.buffer_seconds = buffer_seconds;
}

void StatusDisplay::updateSignalLevel(float signal_db) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.signal_level_db = signal_db;
}

void StatusDisplay::updatePacketSize(size_t size) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.packet_size = size;
}

void StatusDisplay::updateConnectionStatus(bool connected) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.connected = connected;
}

void StatusDisplay::updateFrequency(double freq) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.frequency = freq;
}

void StatusDisplay::updateMode(const std::string& mode) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.mode_text = mode;
}

void StatusDisplay::updateSquelchStatus(bool active) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.squelch_active = active;
}

void StatusDisplay::updateLowcutStatus(bool enabled) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.lowcut_enabled = enabled;
}

void StatusDisplay::updateScannerStatus(bool active, const std::string& name) {
    std::lock_guard<std::mutex> lock(status_mutex);
    current_status.scanning_active = active;
    if (!name.empty()) {
        current_status.scanner_name = name;
    }
}

StatusInfo StatusDisplay::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return current_status;
}

void StatusDisplay::display() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time).count();
    
    // Only update the display at the specified interval
    if (elapsed < update_interval_ms) {
        return;
    }
    
    last_update_time = now;
    
    // If in quiet mode, don't display anything
    if (quiet_mode) {
        return;
    }
    
    // If custom display callback is set, use it
    if (custom_display_callback) {
        std::lock_guard<std::mutex> lock(status_mutex);
        custom_display_callback(current_status);
        return;
    }
    
    // Default display implementation
    std::lock_guard<std::mutex> lock(status_mutex);
    
    // Create signal bar visualization
    float signal_db = current_status.signal_level_db;
    std::string signalBar = std::string(std::max(0, static_cast<int>((signal_db + 30) / 1.9375)), '#') + 
                           std::string(16 - std::max(0, static_cast<int>((signal_db + 30) / 1.9375)), ' ');
    
    // Add squelch indicator
    std::string squelchStatus;
    if (current_status.squelch_active) {
        squelchStatus = "MUTED";
    } else {
        squelchStatus = "OPEN";
    }
    
    // Add connection status
    std::string connectionStatus = current_status.connected ? "Connected" : "Disconnected";
    
    // Format frequency with 3 decimal places
    std::ostringstream freq_stream;
    freq_stream << std::fixed << std::setprecision(3) << current_status.frequency;
    
    // Print the status line in original format
    std::cout << "[rtl_icecast] "
              << freq_stream.str() << " MHz | "
              << current_status.mode_text << " | "
              << "Squelch: " << squelchStatus << " | "
              << "Buffer: " << current_status.buffer_seconds << "s | "
              << "Signal: [" << signalBar << "] " << std::fixed << std::setprecision(1) << signal_db << " dB | ";
    
    if (current_status.packet_size > 0) {
        std::cout << "Last: " << current_status.packet_size << " bytes | ";
    }
    
    std::cout << connectionStatus << std::endl;
}

void StatusDisplay::setQuietMode(bool quiet) {
    quiet_mode = quiet;
}

void StatusDisplay::setUpdateInterval(int interval_ms) {
    update_interval_ms = interval_ms;
}

void StatusDisplay::setCustomDisplayCallback(std::function<void(const StatusInfo&)> callback) {
    custom_display_callback = callback;
}
