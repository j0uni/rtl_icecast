#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <stdexcept>
// Forward declaration for LAME encoder
typedef struct lame_global_struct lame_global_flags;

class MP3EncoderException : public std::runtime_error {
public:
    MP3EncoderException(const std::string& message) : std::runtime_error(message) {}
};

class MP3Encoder {
private:
    lame_global_flags* lame;
    int sample_rate;
    int bitrate;
    int quality;
    std::vector<unsigned char> mp3_buffer;
    
public:
    MP3Encoder(int sampleRate, int bitrate, int quality);
    ~MP3Encoder();
    
    // Prevent copying
    MP3Encoder(const MP3Encoder&) = delete;
    MP3Encoder& operator=(const MP3Encoder&) = delete;
    
    // Allow moving
    MP3Encoder(MP3Encoder&&) noexcept;
    MP3Encoder& operator=(MP3Encoder&&) noexcept;
    
    // Encode PCM audio to MP3
    std::vector<unsigned char> encode(const std::vector<float>& pcm_samples);
    
    // Flush any remaining samples and get the last MP3 frame
    std::vector<unsigned char> flush();
    
    // Getters/setters
    int getSampleRate() const { return sample_rate; }
    int getBitrate() const { return bitrate; }
    int getQuality() const { return quality; }
    
    void setBitrate(int new_bitrate);
    void setQuality(int new_quality);
};
