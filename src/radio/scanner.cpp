#include "scanner.h"
#include <algorithm>
#include <chrono>
#include <iostream>

// Fixed constructor to properly initialize ch_index and freq_index
Scanner::Scanner(const std::vector<ScanList>& scanlist) 
    : ch_index(0), freq_index(0), stepDelayMs(100), active(false) {
    
    // Use insert instead of copy+back_inserter to avoid the bug
    channels.insert(channels.end(), scanlist.begin(), scanlist.end());
    
    std::cout << "[Scanner] scanlist size " << channels.size() << std::endl;
    
    for (size_t i = 0; i < channels.size(); i++) {
        std::cout << "[Scanner] List " << channels[i].name << " with " 
                  << channels[i].frequencies.size() << " frequencies" << std::endl;
        
        for (size_t j = 0; j < channels[i].frequencies.size(); j++) {
            std::cout << "  - " << channels[i].frequencies[j] << " MHz" << std::endl;
        }
    }
}

double Scanner::nextChannel(bool returnFrequency) {
    static auto last_time = std::chrono::steady_clock::now();
    double retval = 0.0;
    
    if (!active || channels.empty()) {
        return retval;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() >= stepDelayMs) {
        last_time = now;
        
        // Move to next frequency in current channel list
        freq_index++;
        
        // If we've reached the end of the current frequency list, move to the next channel
        if (freq_index >= channels[ch_index].frequencies.size()) {
            freq_index = 0;
            ch_index++;
            
            // If we've reached the end of all channels, wrap around
            if (ch_index >= channels.size()) {
                ch_index = 0;
            }
        }
        
        // Return the current frequency if requested
        if (returnFrequency && !channels.empty() && !channels[ch_index].frequencies.empty()) {
            retval = channels[ch_index].frequencies[freq_index];
        }
    }
    
    return retval;
}

double Scanner::previousChannel(bool returnFrequency) {
    static auto last_time = std::chrono::steady_clock::now();
    double retval = 0.0;
    
    if (!active || channels.empty()) {
        return retval;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() >= stepDelayMs) {
        last_time = now;
        
        // Move to previous frequency in current channel list
        if (freq_index > 0) {
            freq_index--;
        } else {
            // If we're at the beginning of the current frequency list, move to the previous channel
            if (ch_index > 0) {
                ch_index--;
            } else {
                // If we're at the beginning of all channels, wrap around to the end
                ch_index = channels.size() - 1;
            }
            
            // Set frequency index to the last frequency in the new channel
            freq_index = channels[ch_index].frequencies.size() - 1;
        }
        
        // Return the current frequency if requested
        if (returnFrequency && !channels.empty() && !channels[ch_index].frequencies.empty()) {
            retval = channels[ch_index].frequencies[freq_index];
        }
    }
    
    return retval;
}

double Scanner::getCurrentFrequency() const {
    if (channels.empty() || ch_index >= channels.size() || 
        channels[ch_index].frequencies.empty() || freq_index >= channels[ch_index].frequencies.size()) {
        return 0.0;
    }
    
    return channels[ch_index].frequencies[freq_index];
}

std::string Scanner::getCurrentName() const {
    if (channels.empty() || ch_index >= channels.size()) {
        return "";
    }
    
    return channels[ch_index].name;
}

void Scanner::setStepDelay(uint16_t delay) {
    stepDelayMs = delay;
}

uint16_t Scanner::getStepDelay() const {
    return stepDelayMs;
}

void Scanner::start() {
    active = true;
}

void Scanner::stop() {
    active = false;
}

bool Scanner::isActive() const {
    return active;
}

void Scanner::addChannel(const ScanList& channel) {
    channels.push_back(channel);
}

void Scanner::removeChannel(size_t index) {
    if (index < channels.size()) {
        channels.erase(channels.begin() + index);
        
        // Adjust current indices if necessary
        if (ch_index >= channels.size()) {
            ch_index = channels.empty() ? 0 : channels.size() - 1;
            freq_index = 0;
        }
    }
}

size_t Scanner::getChannelCount() const {
    return channels.size();
}

const std::vector<ScanList>& Scanner::getChannels() const {
    return channels;
}
