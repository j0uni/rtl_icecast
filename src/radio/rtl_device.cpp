#include "rtl_device.h"
#include <thread>
#include <iostream>
#include <vector>

RTLDevice::RTLDevice() : 
    dev(nullptr),
    sample_rate(1024000),
    center_freq(99.9),
    gain_mode(0),
    tuner_gain(0),
    ppm_correction(0),
    is_running(false) {
}

RTLDevice::~RTLDevice() {
    if (dev) {
        close();
    }
}

bool RTLDevice::open(int index) {
    if (dev) {
        close();
    }
    
    int result = rtlsdr_open(&dev, index);
    if (result < 0 || !dev) {
        std::cerr << "[ERROR] RTLDevice: Failed to open device index " << index << std::endl;
        dev = nullptr;
        return false;
    }
    
    char vendor[256], product[256], serial[256];
    rtlsdr_get_usb_strings(dev, vendor, product, serial);
    
    return true;
}

void RTLDevice::close() {
    if (dev) {
        if (is_running) {
            stop_streaming();
        }
        rtlsdr_close(dev);
        dev = nullptr;
    }
}

void RTLDevice::set_sample_rate(int rate) {
    if (!dev) {
        std::cerr << "[ERROR] RTLDevice: Device not open" << std::endl;
        return;
    }
    
    int result = rtlsdr_set_sample_rate(dev, rate);
    if (result < 0) {
        std::cerr << "[ERROR] RTLDevice: Failed to set sample rate to " << rate << " Hz" << std::endl;
        return;
    }
    
    sample_rate = rate;
}

void RTLDevice::set_center_freq(double freq_mhz) {
    if (!dev) {
        std::cerr << "[ERROR] RTLDevice: Device not open" << std::endl;
        return;
    }
    
    uint32_t freq_hz = static_cast<uint32_t>(freq_mhz * 1e6);
    
    // Try multiple times to set the frequency to ensure PLL lock
    const int max_attempts = 5;
    int result = -1;
    
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = rtlsdr_set_center_freq(dev, freq_hz);
        
        if (result >= 0) {
            // Reset the buffer to clear any old data
            rtlsdr_reset_buffer(dev);
            center_freq = freq_mhz;
            return;
        }
    }
    
    std::cerr << "[ERROR] RTLDevice: Failed to set center frequency to " 
              << (freq_hz / 1e6) << " MHz after " << max_attempts << " attempts" << std::endl;
}

void RTLDevice::set_gain_mode(int mode) {
    if (!dev) {
        std::cerr << "[ERROR] RTLDevice: Device not open" << std::endl;
        return;
    }
    
    int result = rtlsdr_set_tuner_gain_mode(dev, mode);
    if (result < 0) {
        std::cerr << "[ERROR] RTLDevice: Failed to set gain mode to " << mode << std::endl;
        return;
    }
    
    gain_mode = mode;
}

void RTLDevice::set_tuner_gain(int gain) {
    if (!dev) {
        std::cerr << "[ERROR] RTLDevice: Device not open" << std::endl;
        return;
    }
    
    int result = rtlsdr_set_tuner_gain(dev, gain);
    if (result < 0) {
        std::cerr << "[ERROR] RTLDevice: Failed to set gain to " << gain << std::endl;
        return;
    }
    
    tuner_gain = gain;
}

void RTLDevice::set_ppm_correction(int ppm) {
    if (!dev) {
        std::cerr << "[ERROR] RTLDevice: Device not open" << std::endl;
        return;
    }
    
    // If PPM is 0, just store the value but don't try to set it
    // This avoids the error message for the common case of no correction
    if (ppm == 0) {
        ppm_correction = ppm;
        return;
    }
    
    int result = rtlsdr_set_freq_correction(dev, ppm);
    if (result < 0) {
        std::cerr << "[ERROR] RTLDevice: Failed to set PPM correction to " << ppm << " (non-fatal, continuing)" << std::endl;
    }
    
    ppm_correction = ppm;
}

void RTLDevice::set_callback(std::function<void(unsigned char*, uint32_t)> cb) {
    callback = cb;
}

void RTLDevice::start_streaming() {
    if (!dev) {
        throw RTLException("Device not opened");
    }
    
    if (!callback) {
        throw RTLException("No callback set");
    }
    
    if (is_running) {
        return;
    }
    
    // Reset buffer
    rtlsdr_reset_buffer(dev);
    
    // Start the async read thread
    is_running = true;
    
    // Use a lambda to bridge the C callback to our C++ callback
    rtlsdr_read_async(dev, 
        [](unsigned char* buf, uint32_t len, void* ctx) {
            RTLDevice* device = static_cast<RTLDevice*>(ctx);
            if (device && device->callback) {
                device->callback(buf, len);
            }
        }, 
        this, 0, 0);
}

void RTLDevice::stop_streaming() {
    if (!dev || !is_running) {
        return;
    }
    
    rtlsdr_cancel_async(dev);
    is_running = false;
}

int RTLDevice::get_device_count() {
    return rtlsdr_get_device_count();
}

std::string RTLDevice::get_device_name(int index) {
    const char* name = rtlsdr_get_device_name(index);
    return name ? std::string(name) : "Unknown device";
}
