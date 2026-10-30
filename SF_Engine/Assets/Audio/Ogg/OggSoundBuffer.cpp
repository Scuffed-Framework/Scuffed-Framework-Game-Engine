#include "OggSoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
    #include <OpenAL/al.h>
#else
    #include <al.h>
#endif

extern "C"
{
#include "stb_vorbis.c"
}

#include <iostream>

namespace SF::Engine
{
    void OggSoundBuffer::Load(SoundBuffer &soundBuffer, const DataInput &input)
    {
        int channels = 0, sampleRate = 0;
        short *output = nullptr;

        const int framesPerChannel =
                stb_vorbis_decode_filename(input.file->GetFullPath().c_str(), &channels, &sampleRate, &output);

        if (framesPerChannel <= 0 || output == nullptr)
        {
            std::cerr << "Failed to decode OGG file: " << input.file->GetFullPath() << std::endl;
            return;
        }

        if (channels < 1 || channels > 2)
        {
            std::cerr << "Unsupported OGG channel count (" << channels << "), only mono/stereo supported" << std::endl;
            free(output);
            return;
        }

        std::vector<int16_t> pcm(output, output + static_cast<size_t>(framesPerChannel) * channels);
        free(output); // stb allocates with malloc; we own a copy now

        soundBuffer.SetPCM({.Samples      = std::move(pcm),
                            .SampleRate   = static_cast<uint32_t>(sampleRate),
                            .ChannelCount = static_cast<uint8_t>(channels)});
    }
} // namespace SF::Engine
