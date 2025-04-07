#include "demodulator.h"
#include <liquid/liquid.h>
#include <cmath>

// Implementation details for FMDemodulator
struct FMDemodulator::Impl {
    std::complex<float> prev_sample;
    float prev_demod;
    float dc_block_alpha;
    float dc_avg;
    float deemph_alpha;
    float deemph_prev;
    bool use_deemphasis;
    float deviation;
    float sample_rate;
    
    Impl(float sampleRate, ModulationMode mode) : 
        prev_sample(1.0f, 0.0f),
        prev_demod(0.0f),
        dc_block_alpha(0.01f),
        dc_avg(0.0f),
        deemph_alpha(0.0f),
        deemph_prev(0.0f),
        use_deemphasis(true),
        deviation(75000.0f), // Default to WFM
        sample_rate(sampleRate) {
        
        // Initialize with the provided mode
        setMode(mode, sampleRate);
    }
    
    void setMode(ModulationMode mode, float sampleRate) {
        sample_rate = sampleRate;
        
        // Set deviation based on mode
        switch (mode) {
            case ModulationMode::WFM_MODE:
                deviation = 75000.0f; // 75 kHz for WFM
                use_deemphasis = true;
                break;
            case ModulationMode::NFM_MODE:
                deviation = 12500.0f; // 12.5 kHz for NFM
                use_deemphasis = true;
                break;
            case ModulationMode::AM_MODE:
                // Not applicable for AM, but set a value anyway
                deviation = 1.0f;
                use_deemphasis = false;
                break;
        }
        
        // Update de-emphasis filter for new sample rate
        float time_constant = (mode == ModulationMode::WFM_MODE) ? 75e-6f : 50e-6f; // 75µs for WFM, 50µs for NFM
        deemph_alpha = 1.0f - expf(-1.0f / (time_constant * sample_rate));
    }
    
    void reset() {
        prev_sample = std::complex<float>(1.0f, 0.0f);
        prev_demod = 0.0f;
        dc_avg = 0.0f;
        deemph_prev = 0.0f;
    }
    
    float demodulate(std::complex<float> sample) {
        // Phase difference demodulation with unwrapping
        float phase = std::arg(sample * std::conj(prev_sample));
        
        // Phase unwrapping for large frequency deviations
        if (phase > M_PI) phase -= 2.0f * M_PI;
        else if (phase < -M_PI) phase += 2.0f * M_PI;
        
        // Convert phase to audio sample
        float demod = phase * (sample_rate / (2.0f * M_PI * deviation));
        
        // DC blocking filter
        dc_avg = dc_avg * (1.0f - dc_block_alpha) + demod * dc_block_alpha;
        float dc_blocked = demod - dc_avg;
        
        // De-emphasis filter (1st order IIR low-pass)
        float result;
        if (use_deemphasis) {
            deemph_prev = deemph_prev + deemph_alpha * (dc_blocked - deemph_prev);
            result = deemph_prev;
        } else {
            result = dc_blocked;
        }
        
        // Update state for next sample
        prev_sample = sample;
        prev_demod = demod;
        
        return result;
    }
};

// FMDemodulator implementation
FMDemodulator::FMDemodulator(float sampleRate, ModulationMode mode)
    : pImpl(new Impl(sampleRate, mode)) {
}

FMDemodulator::~FMDemodulator() = default;

// Move constructor and assignment
FMDemodulator::FMDemodulator(FMDemodulator&&) noexcept = default;
FMDemodulator& FMDemodulator::operator=(FMDemodulator&&) noexcept = default;

void FMDemodulator::setMode(ModulationMode mode, float sampleRate) {
    pImpl->setMode(mode, sampleRate);
}

void FMDemodulator::reset() {
    pImpl->reset();
}

float FMDemodulator::demodulate(std::complex<float> sample) {
    return pImpl->demodulate(sample);
}

float FMDemodulator::demodulate(std::complex<float> prev, std::complex<float> curr) {
    pImpl->prev_sample = prev;
    return pImpl->demodulate(curr);
}

// AMDemodulator implementation
AMDemodulator::AMDemodulator(float alpha)
    : dc_block_alpha(alpha), dc_avg(0.0f) {
}

void AMDemodulator::reset() {
    dc_avg = 0.0f;
}

float AMDemodulator::demodulate(std::complex<float> sample) {
    // Magnitude demodulation
    float mag = std::abs(sample);
    
    // DC blocking filter
    dc_avg = dc_avg * (1.0f - dc_block_alpha) + mag * dc_block_alpha;
    float dc_blocked = mag - dc_avg;
    
    return dc_blocked;
}

// AudioFilter implementation
AudioFilter::AudioFilter(float cutoffFreq, int filterOrder, float sampleRate)
    : filter(nullptr), enabled(false), cutoff_freq(cutoffFreq), order(filterOrder), sample_rate(sampleRate) {
    
    // Create a Butterworth high-pass filter
    float fc = cutoff_freq / sample_rate; // Normalized cutoff frequency
    filter = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_HIGHPASS, 
                                          LIQUID_IIRDES_SOS, order, fc, 0.0f, 1.0f, 1.0f);
}

AudioFilter::~AudioFilter() {
    if (filter) {
        iirfilt_rrrf_destroy(filter);
    }
}

// Move constructor
AudioFilter::AudioFilter(AudioFilter&& other) noexcept
    : filter(other.filter), enabled(other.enabled), 
      cutoff_freq(other.cutoff_freq), order(other.order), sample_rate(other.sample_rate) {
    other.filter = nullptr;
}

// Move assignment
AudioFilter& AudioFilter::operator=(AudioFilter&& other) noexcept {
    if (this != &other) {
        if (filter) {
            iirfilt_rrrf_destroy(filter);
        }
        
        filter = other.filter;
        enabled = other.enabled;
        cutoff_freq = other.cutoff_freq;
        order = other.order;
        sample_rate = other.sample_rate;
        
        other.filter = nullptr;
    }
    return *this;
}

void AudioFilter::setCutoffFrequency(float freq) {
    if (freq == cutoff_freq) {
        return;
    }
    
    cutoff_freq = freq;
    
    // Recreate the filter with the new cutoff frequency
    if (filter) {
        iirfilt_rrrf_destroy(filter);
    }
    
    float fc = cutoff_freq / sample_rate; // Normalized cutoff frequency
    filter = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_HIGHPASS, 
                                          LIQUID_IIRDES_SOS, order, fc, 0.0f, 1.0f, 1.0f);
}

void AudioFilter::setEnabled(bool enable) {
    enabled = enable;
}

bool AudioFilter::isEnabled() const {
    return enabled;
}

void AudioFilter::setOrder(int newOrder) {
    if (newOrder == order) {
        return;
    }
    
    order = newOrder;
    
    // Recreate the filter with the new order
    if (filter) {
        iirfilt_rrrf_destroy(filter);
    }
    
    float fc = cutoff_freq / sample_rate; // Normalized cutoff frequency
    filter = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_HIGHPASS, 
                                          LIQUID_IIRDES_SOS, order, fc, 0.0f, 1.0f, 1.0f);
}

void AudioFilter::setSampleRate(float rate) {
    if (rate == sample_rate) {
        return;
    }
    
    sample_rate = rate;
    
    // Recreate the filter with the new sample rate
    if (filter) {
        iirfilt_rrrf_destroy(filter);
    }
    
    float fc = cutoff_freq / sample_rate; // Normalized cutoff frequency
    filter = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER, LIQUID_IIRDES_HIGHPASS, 
                                          LIQUID_IIRDES_SOS, order, fc, 0.0f, 1.0f, 1.0f);
}

float AudioFilter::process(float sample) {
    if (!enabled || !filter) {
        return sample;
    }
    
    float output;
    iirfilt_rrrf_execute(filter, sample, &output);
    return output;
}

void AudioFilter::reset() {
    if (filter) {
        iirfilt_rrrf_reset(filter);
    }
}
