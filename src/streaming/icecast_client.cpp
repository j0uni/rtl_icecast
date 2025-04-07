#include "icecast_client.h"
#include <shout/shout.h>
#include <iostream>
#include <thread>
#include <sstream>
#include <iomanip>

IcecastClient::IcecastClient(const std::string& host, int port, const std::string& mount,
                           const std::string& password, const std::string& username,
                           const std::string& protocol, const std::string& format)
    : shout(nullptr), host(host), port(port), mount(mount), 
      password(password), username(username), protocol(protocol), format(format),
      station_title("RTL-SDR Radio"), connected(false),
      reconnect_attempts(5), reconnect_delay_ms(2000) {
    
    // Initialize shout
    shout = shout_new();
    if (!shout) {
        throw StreamingException("Failed to allocate shout_t");
    }
    
    // Configure shout
    if (shout_set_host(shout, host.c_str()) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting hostname: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_protocol(shout, protocol == "http" ? SHOUT_PROTOCOL_HTTP : SHOUT_PROTOCOL_ICY) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting protocol: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting port: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_password(shout, password.c_str()) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting password: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_mount(shout, mount.c_str()) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting mount point: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_user(shout, username.c_str()) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting user: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_format(shout, format == "mp3" ? SHOUT_FORMAT_MP3 : SHOUT_FORMAT_OGG) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting format: " + std::string(shout_get_error(shout)));
    }
    
    // Set content type based on format
    if (format == "mp3") {
        if (shout_set_content_format(shout, SHOUT_FORMAT_MP3, SHOUT_USAGE_AUDIO, NULL) != SHOUTERR_SUCCESS) {
            throw StreamingException("Error setting content format: " + std::string(shout_get_error(shout)));
        }
    } else {
        if (shout_set_content_format(shout, SHOUT_FORMAT_OGG, SHOUT_USAGE_AUDIO, NULL) != SHOUTERR_SUCCESS) {
            throw StreamingException("Error setting content format: " + std::string(shout_get_error(shout)));
        }
    }
    
    // Set metadata
    if (shout_set_name(shout, station_title.c_str()) != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting station name: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_description(shout, "RTL-SDR FM Radio Stream") != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting description: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_genre(shout, "Radio") != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting genre: " + std::string(shout_get_error(shout)));
    }
    
    if (shout_set_url(shout, "http://github.com/j0uni/rtl_icecast") != SHOUTERR_SUCCESS) {
        throw StreamingException("Error setting URL: " + std::string(shout_get_error(shout)));
    }
}

IcecastClient::~IcecastClient() {
    if (connected) {
        disconnect();
    }
    
    if (shout) {
        shout_free(shout);
        shout = nullptr;
    }
}

// Move constructor
IcecastClient::IcecastClient(IcecastClient&& other) noexcept
    : shout(other.shout), host(std::move(other.host)), port(other.port),
      mount(std::move(other.mount)), password(std::move(other.password)),
      username(std::move(other.username)), protocol(std::move(other.protocol)),
      format(std::move(other.format)), station_title(std::move(other.station_title)),
      connected(other.connected), reconnect_attempts(other.reconnect_attempts),
      reconnect_delay_ms(other.reconnect_delay_ms),
      connection_callback(std::move(other.connection_callback)) {
    
    other.shout = nullptr;
    other.connected = false;
}

// Move assignment
IcecastClient& IcecastClient::operator=(IcecastClient&& other) noexcept {
    if (this != &other) {
        if (connected) {
            disconnect();
        }
        
        if (shout) {
            shout_free(shout);
        }
        
        shout = other.shout;
        host = std::move(other.host);
        port = other.port;
        mount = std::move(other.mount);
        password = std::move(other.password);
        username = std::move(other.username);
        protocol = std::move(other.protocol);
        format = std::move(other.format);
        station_title = std::move(other.station_title);
        connected = other.connected;
        reconnect_attempts = other.reconnect_attempts;
        reconnect_delay_ms = other.reconnect_delay_ms;
        connection_callback = std::move(other.connection_callback);
        
        other.shout = nullptr;
        other.connected = false;
    }
    return *this;
}

bool IcecastClient::connect() {
    if (!shout) {
        std::cerr << "[ERROR] IcecastClient: Cannot connect - shout is null" << std::endl;
        return false;
    }
    
    if (connected) {
        return true;
    }
    
    // Verify all required parameters are set
    if (host.empty()) {
        std::cerr << "[ERROR] IcecastClient: Host is empty" << std::endl;
        return false;
    }
    
    if (mount.empty()) {
        std::cerr << "[ERROR] IcecastClient: Mount point is empty" << std::endl;
        return false;
    }
    
    if (password.empty()) {
        std::cerr << "[ERROR] IcecastClient: Password is empty" << std::endl;
        return false;
    }
    
    // Try to open the connection
    int result = shout_open(shout);
    
    // Check the result
    if (result == SHOUTERR_SUCCESS) {
        connected = true;
        
        // Notify callback if set
        if (connection_callback) {
            connection_callback(true);
        }
        
        return true;
    } else {
        std::cerr << "[ERROR] IcecastClient: Error connecting to Icecast: " 
                  << shout_get_error(shout) << " (code: " << result << ")" << std::endl;
        
        // Additional error information
        if (result == SHOUTERR_NOCONNECT) {
            std::cerr << "[ERROR] IcecastClient: Could not connect to server. Check if Icecast is running." << std::endl;
        } else if (result == SHOUTERR_NOLOGIN) {
            std::cerr << "[ERROR] IcecastClient: Login failed. Check username/password and mount point." << std::endl;
        } else if (result == SHOUTERR_SOCKET) {
            std::cerr << "[ERROR] IcecastClient: Socket error. Check network connection." << std::endl;
        }
        
        return false;
    }
}

void IcecastClient::disconnect() {
    if (!shout || !connected) {
        return;
    }
    
    shout_close(shout);
    connected = false;
    
    // Notify callback if set
    if (connection_callback) {
        connection_callback(false);
    }
}

bool IcecastClient::checkConnection() {
    if (!shout) {
        return false;
    }
    
    if (!connected) {
        return false;
    }
    
    int err = shout_get_errno(shout);
    if (err != SHOUTERR_SUCCESS && err != SHOUTERR_BUSY) {
        std::cerr << "Icecast connection error: " << shout_get_error(shout) << std::endl;
        connected = false;
        
        // Notify callback if set
        if (connection_callback) {
            connection_callback(false);
        }
        
        return false;
    }
    
    return true;
}

bool IcecastClient::reconnect() {
    if (!shout) {
        return false;
    }
    
    disconnect();
    
    for (int attempt = 0; attempt < reconnect_attempts; ++attempt) {
        std::cout << "Reconnecting to Icecast (attempt " << (attempt + 1) << "/" << reconnect_attempts << ")..." << std::endl;
        
        if (connect()) {
            std::cout << "Reconnected to Icecast successfully!" << std::endl;
            return true;
        }
        
        // Wait before next attempt
        std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay_ms));
    }
    
    std::cerr << "Failed to reconnect to Icecast after " << reconnect_attempts << " attempts" << std::endl;
    return false;
}

bool IcecastClient::sendData(const std::vector<unsigned char>& data) {
    if (!shout || !connected) {
        std::cerr << "[ERROR] IcecastClient: Cannot send data - " 
                  << (!shout ? "shout is null" : "not connected") << std::endl;
        return false;
    }
    
    int result = shout_send(shout, data.data(), data.size());
    if (result != SHOUTERR_SUCCESS) {
        std::cerr << "[ERROR] IcecastClient: Error sending data to Icecast: " << shout_get_error(shout) << std::endl;
        connected = false;
        
        // Notify callback if set
        if (connection_callback) {
            connection_callback(false);
        }
        
        return false;
    }
    
    // Wait for data to be sent
    shout_sync(shout);
    
    return true;
}

void IcecastClient::setStationTitle(const std::string& title) {
    station_title = title;
    
    if (shout) {
        shout_set_name(shout, title.c_str());
    }
}

void IcecastClient::updateMetadata(const std::string& title) {
    if (!shout || !connected) {
        return;
    }
    
    shout_metadata_t* metadata = shout_metadata_new();
    if (!metadata) {
        std::cerr << "Error creating metadata" << std::endl;
        return;
    }
    
    if (shout_metadata_add(metadata, "song", title.c_str()) != SHOUTERR_SUCCESS) {
        std::cerr << "Error adding metadata" << std::endl;
        shout_metadata_free(metadata);
        return;
    }
    
    if (shout_set_metadata(shout, metadata) != SHOUTERR_SUCCESS) {
        std::cerr << "Error setting metadata: " << shout_get_error(shout) << std::endl;
    }
    
    shout_metadata_free(metadata);
}

void IcecastClient::updateMetadata(double frequency, float signal_level) {
    if (!shout || !connected) {
        return;
    }
    
    // Format frequency with 1 decimal place
    std::ostringstream freq_stream;
    freq_stream << std::fixed << std::setprecision(1) << frequency;
    
    // Format signal level
    std::ostringstream signal_stream;
    signal_stream << std::fixed << std::setprecision(1) << signal_level;
    
    // Create title string
    std::string title = station_title + " - " + freq_stream.str() + " MHz";
    if (signal_level > -120.0f) {
        title += " (Signal: " + signal_stream.str() + " dB)";
    }
    
    updateMetadata(title);
}

void IcecastClient::setConnectionCallback(std::function<void(bool)> callback) {
    connection_callback = callback;
}

void IcecastClient::initializeLibrary() {
    shout_init();
}

void IcecastClient::shutdownLibrary() {
    shout_shutdown();
}
