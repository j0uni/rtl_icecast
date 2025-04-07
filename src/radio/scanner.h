#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct ScanList {
    std::string name;
    std::vector<double> frequencies;
};

class Scanner {
private:
    std::vector<ScanList> channels;
    std::size_t ch_index;
    std::size_t freq_index;
    uint16_t stepDelayMs;
    bool active;

public:
    // Fixed constructor to properly initialize ch_index
    Scanner(const std::vector<ScanList>& scanlist);
    
    // Get next channel
    double nextChannel(bool returnFrequency = true);
    
    // Get previous channel
    double previousChannel(bool returnFrequency = true);
    
    // Get current channel info
    double getCurrentFrequency() const;
    std::string getCurrentName() const;
    
    // Configuration
    void setStepDelay(uint16_t delay);
    uint16_t getStepDelay() const;
    
    // Scanner control
    void start();
    void stop();
    bool isActive() const;
    
    // Channel management
    void addChannel(const ScanList& channel);
    void removeChannel(size_t index);
    size_t getChannelCount() const;
    
    // Get all channels
    const std::vector<ScanList>& getChannels() const;
};
