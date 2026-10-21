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
        int channels = 0;
        int sampleRate = 0;
        short *output = nullptr;

        int samplesPerChannel = stb_vorbis_decode_filename(input.file->GetName().c_str(), &channels,
                                                             &sampleRate, &output);

        if (samplesPerChannel < 0 || output == nullptr)
        {
            std::cerr << "Failed to decode OGG file: " << input.file << std::endl;
            return;
        }

        if (channels < 1 || channels > 2)
        {
            std::cerr << "Unsupported OGG channel count (" << channels
                       << ") in " << input.file << ", only mono/stereo supported" << std::endl;
            free(output);
            return;
        }

        ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        ALsizei size = static_cast<ALsizei>(samplesPerChannel) * channels * static_cast<ALsizei>(sizeof(short));

        alBufferData(soundBuffer.GetBuffer(), format, output, size, sampleRate);

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to buffer OGG data for " << input.file << ": " << error << std::endl;
        }

        free(output);
    }

    void OggSoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file)
    {
        // stb_vorbis is a decoder only; no Ogg Vorbis encoder is bundled.
        (void)soundBuffer;
        std::cerr << "OggSoundBuffer::Write is not supported (stb_vorbis has no encoder): "
                  << file << std::endl;
    }
}
