#pragma once

#include <complex>
#include <cmath>
#include <memory>
#include "../config/config.h"

// Forward declarations
struct iirfilt_rrrf_s;
typedef struct iirfilt_rrrf_s * iirfilt_rrrf;

class FMDemodulator {
private:
    // Private implementation details
    struct Impl;
    std::unique_ptr<Impl> pImpl;  // PIMPL idiom for better encapsulation

public:
    // Public interface
    FMDemodulator(float sampleRate, ModulationMode mode);
    ~FMDemodulator();
    
    // Prevent copying
    FMDemodulator(const FMDemodulator&) = delete;
    FMDemodulator& operator=(const FMDemodulator&) = delete;
    
    // Allow moving
    FMDemodulator(FMDemodulator&&) noexcept;
    FMDemodulator& operator=(FMDemodulator&&) noexcept;
    
    void setMode(ModulationMode mode, float sampleRate);
    void reset();
    float demodulate(std::complex<float> sample);
    
    // For compatibility with the old interface
    float demodulate(std::complex<float> prev, std::complex<float> curr);
};

class AMDemodulator {
private:
    float dc_block_alpha;
    float dc_avg;
    
public:
    AMDemodulator(float alpha = 0.01f);
    
    // Getters
    float getDcBlockAlpha() const { return dc_block_alpha; }
    float getDcAvg() const { return dc_avg; }
    
    // Setters
    void setDcBlockAlpha(float alpha) { dc_block_alpha = alpha; }
    
    // Processing
    void reset();
    float demodulate(std::complex<float> sample);
};

class AudioFilter {
private:
    iirfilt_rrrf filter;
    bool enabled;
    float cutoff_freq;
    int order;
    float sample_rate;
    
public:
    AudioFilter(float cutoffFreq, int filterOrder, float sampleRate);
    ~AudioFilter();
    
    // Prevent copying
    AudioFilter(const AudioFilter&) = delete;
    AudioFilter& operator=(const AudioFilter&) = delete;
    
    // Allow moving
    AudioFilter(AudioFilter&&) noexcept;
    AudioFilter& operator=(AudioFilter&&) noexcept;
    
    // Getters
    float getCutoffFrequency() const { return cutoff_freq; }
    int getOrder() const { return order; }
    float getSampleRate() const { return sample_rate; }
    bool isEnabled() const;
    
    // Setters
    void setCutoffFrequency(float freq);
    void setEnabled(bool enable);
    void setOrder(int newOrder);
    void setSampleRate(float rate);
    
    // Processing
    float process(float sample);
    void reset();
};
