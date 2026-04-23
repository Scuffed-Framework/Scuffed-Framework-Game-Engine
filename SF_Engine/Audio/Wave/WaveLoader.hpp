#include <cstdint>

namespace SF::Engine
{
    // WAV file header structure
    struct WAVHeader
    {
        char riff[4];
        int32_t fileSize;
        char wave[4];
        char fmt[4];
        int32_t fmtSize;
        int16_t audioFormat;
        int16_t numChannels;
        int32_t sampleRate;
        int32_t byteRate;
        int16_t blockAlign;
        int16_t bitsPerSample;
        char data[4];
        int32_t dataSize;
    };
}  // namespace SF::Engine
