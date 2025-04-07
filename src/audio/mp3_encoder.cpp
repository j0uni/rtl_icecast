#include "mp3_encoder.h"
#include <lame/lame.h>
#include <stdexcept>
#include <cstring>

MP3Encoder::MP3Encoder(int sampleRate, int bitrate, int quality)
    : lame(nullptr), sample_rate(sampleRate), bitrate(bitrate), quality(quality) {
    
    // Initialize LAME
    lame = lame_init();
    if (!lame) {
        throw MP3EncoderException("Failed to initialize LAME encoder");
    }
    
    // Configure LAME
    lame_set_in_samplerate(lame, sample_rate);
    lame_set_out_samplerate(lame, sample_rate);
    lame_set_num_channels(lame, 1); // Mono
    lame_set_brate(lame, bitrate);
    lame_set_quality(lame, quality); // 0=best, 9=worst
    
    // Initialize the encoder
    if (lame_init_params(lame) < 0) {
        lame_close(lame);
        throw MP3EncoderException("Failed to initialize LAME parameters");
    }
    
    // Allocate MP3 buffer (1.25x PCM size is recommended by LAME docs)
    mp3_buffer.resize(sample_rate * 2); // Should be enough for 1 second of audio
}

MP3Encoder::~MP3Encoder() {
    if (lame) {
        lame_close(lame);
        lame = nullptr;
    }
}

// Move constructor
MP3Encoder::MP3Encoder(MP3Encoder&& other) noexcept
    : lame(other.lame), sample_rate(other.sample_rate), 
      bitrate(other.bitrate), quality(other.quality),
      mp3_buffer(std::move(other.mp3_buffer)) {
    other.lame = nullptr;
}

// Move assignment
MP3Encoder& MP3Encoder::operator=(MP3Encoder&& other) noexcept {
    if (this != &other) {
        if (lame) {
            lame_close(lame);
        }
        
        lame = other.lame;
        sample_rate = other.sample_rate;
        bitrate = other.bitrate;
        quality = other.quality;
        mp3_buffer = std::move(other.mp3_buffer);
        
        other.lame = nullptr;
    }
    return *this;
}

std::vector<unsigned char> MP3Encoder::encode(const std::vector<float>& pcm_samples) {
    if (!lame) {
        throw MP3EncoderException("Encoder not initialized");
    }
    
    // Ensure buffer is large enough (1.25x PCM size is recommended by LAME docs)
    size_t min_buffer_size = pcm_samples.size() * 5 / 4 + 7200; // Add 7200 bytes for LAME header/footer
    if (mp3_buffer.size() < min_buffer_size) {
        mp3_buffer.resize(min_buffer_size);
    }
    
    // Encode PCM to MP3
    int encoded_size = lame_encode_buffer_ieee_float(
        lame,
        pcm_samples.data(),  // Left channel (mono)
        nullptr,             // Right channel (none for mono)
        pcm_samples.size(),  // Number of samples
        mp3_buffer.data(),   // Output buffer
        mp3_buffer.size()    // Output buffer size
    );
    
    if (encoded_size < 0) {
        throw MP3EncoderException("MP3 encoding failed with error: " + std::to_string(encoded_size));
    }
    
    // Return only the actual encoded data
    return std::vector<unsigned char>(mp3_buffer.begin(), mp3_buffer.begin() + encoded_size);
}

std::vector<unsigned char> MP3Encoder::flush() {
    if (!lame) {
        throw MP3EncoderException("Encoder not initialized");
    }
    
    // Flush any remaining samples
    int encoded_size = lame_encode_flush(
        lame,
        mp3_buffer.data(),   // Output buffer
        mp3_buffer.size()    // Output buffer size
    );
    
    if (encoded_size < 0) {
        throw MP3EncoderException("MP3 flush failed with error: " + std::to_string(encoded_size));
    }
    
    // Return only the actual encoded data
    return std::vector<unsigned char>(mp3_buffer.begin(), mp3_buffer.begin() + encoded_size);
}

void MP3Encoder::setBitrate(int new_bitrate) {
    if (bitrate == new_bitrate) {
        return;
    }
    
    bitrate = new_bitrate;
    
    // Reinitialize LAME with new bitrate
    if (lame) {
        lame_set_brate(lame, bitrate);
        lame_init_params(lame);
    }
}

void MP3Encoder::setQuality(int new_quality) {
    if (quality == new_quality) {
        return;
    }
    
    quality = new_quality;
    
    // Reinitialize LAME with new quality
    if (lame) {
        lame_set_quality(lame, quality);
        lame_init_params(lame);
    }
}
