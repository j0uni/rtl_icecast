#pragma once

#include <rtl-sdr.h>
#include <functional>
#include <string>
#include <stdexcept>

class RTLException : public std::runtime_error {
public:
    RTLException(const std::string& message) : std::runtime_error(message) {}
};

class RTLDevice {
private:
    rtlsdr_dev_t* dev;
    int sample_rate;
    double center_freq;
    int gain_mode;
    int tuner_gain;
    int ppm_correction;
    
    // Callback related members
    std::function<void(unsigned char*, uint32_t)> callback;
    bool is_running;
    
public:
    RTLDevice();
    ~RTLDevice();
    
    // Prevent copying
    RTLDevice(const RTLDevice&) = delete;
    RTLDevice& operator=(const RTLDevice&) = delete;
    
    // Device management
    bool open(int index);
    void close();
    bool is_open() const { return dev != nullptr; }
    
    // Configuration
    void set_sample_rate(int rate);
    void set_center_freq(double freq_mhz);
    void set_gain_mode(int mode);
    void set_tuner_gain(int gain);
    void set_ppm_correction(int ppm);
    
    // Getters
    int get_sample_rate() const { return sample_rate; }
    double get_center_freq() const { return center_freq; }
    int get_gain_mode() const { return gain_mode; }
    int get_tuner_gain() const { return tuner_gain; }
    int get_ppm_correction() const { return ppm_correction; }
    
    // Streaming
    void set_callback(std::function<void(unsigned char*, uint32_t)> cb);
    void start_streaming();
    void stop_streaming();
    
    // Static methods
    static int get_device_count();
    static std::string get_device_name(int index);
};
