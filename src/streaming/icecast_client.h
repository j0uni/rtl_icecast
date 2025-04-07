#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

// Forward declaration for shout_t
typedef struct shout shout_t;

class StreamingException : public std::runtime_error {
public:
    StreamingException(const std::string& message) : std::runtime_error(message) {}
};

class IcecastClient {
private:
    shout_t* shout;
    std::string host;
    int port;
    std::string mount;
    std::string password;
    std::string username;
    std::string protocol;
    std::string format;
    std::string station_title;
    bool connected;
    
    int reconnect_attempts;
    int reconnect_delay_ms;
    
    // Callback for status updates
    std::function<void(bool)> connection_callback;
    
public:
    IcecastClient(const std::string& host, int port, const std::string& mount,
                 const std::string& password, const std::string& username = "source",
                 const std::string& protocol = "http", const std::string& format = "mp3");
    ~IcecastClient();
    
    // Prevent copying
    IcecastClient(const IcecastClient&) = delete;
    IcecastClient& operator=(const IcecastClient&) = delete;
    
    // Allow moving
    IcecastClient(IcecastClient&&) noexcept;
    IcecastClient& operator=(IcecastClient&&) noexcept;
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const { return connected; }
    bool checkConnection();
    bool reconnect();
    
    // Streaming
    bool sendData(const std::vector<unsigned char>& data);
    
    // Metadata
    void setStationTitle(const std::string& title);
    void updateMetadata(const std::string& title);
    void updateMetadata(double frequency, float signal_level);
    
    // Status callback
    void setConnectionCallback(std::function<void(bool)> callback);
    
    // Configuration
    void setReconnectAttempts(int attempts) { reconnect_attempts = attempts; }
    void setReconnectDelay(int delay_ms) { reconnect_delay_ms = delay_ms; }
    
    // Getters for configuration parameters
    const std::string& getHost() const { return host; }
    int getPort() const { return port; }
    const std::string& getMount() const { return mount; }
    const std::string& getUsername() const { return username; }
    const std::string& getProtocol() const { return protocol; }
    const std::string& getFormat() const { return format; }
    const std::string& getStationTitle() const { return station_title; }
    int getReconnectAttempts() const { return reconnect_attempts; }
    int getReconnectDelay() const { return reconnect_delay_ms; }
    
    // Static initialization
    static void initializeLibrary();
    static void shutdownLibrary();
};
