#include "Mp3SoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.hpp"

#include <iostream>
#include <vector>

namespace SF::Engine
{
    void Mp3SoundBuffer::Load(SoundBuffer &soundBuffer, const DataInput &input)
    {
        drmp3 mp3;
        if (!drmp3_init_file(&mp3, input.file->GetName().c_str(), nullptr))
        {
            std::cerr << "Failed to open MP3 file: " << input.file << std::endl;
            return;
        }

        if (mp3.channels < 1 || mp3.channels > 2)
        {
            std::cerr << "Unsupported MP3 channel count (" << mp3.channels
                       << ") in " << input.file << ", only mono/stereo supported" << std::endl;
            drmp3_uninit(&mp3);
            return;
        }

        drmp3_uint64 frameCount = drmp3_get_pcm_frame_count(&mp3);
        std::vector<int16_t> pcm(static_cast<size_t>(frameCount) * mp3.channels);

        // Re-init required after drmp3_get_pcm_frame_count, which consumes the stream.
        drmp3_uninit(&mp3);
        if (!drmp3_init_file(&mp3, input.file->GetName().c_str(), nullptr))
        {
            std::cerr << "Failed to reopen MP3 file: " << input.file << std::endl;
            return;
        }

        drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(&mp3, frameCount, pcm.data());

        ALenum format = (mp3.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        alBufferData(soundBuffer.GetBuffer(), format, pcm.data(),
                     static_cast<ALsizei>(framesRead * mp3.channels * sizeof(int16_t)),
                     static_cast<ALsizei>(mp3.sampleRate));

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to buffer MP3 data for " << input.file << ": " << error << std::endl;
        }

        drmp3_uninit(&mp3);
    }

    void Mp3SoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file)
    {
        // dr_mp3 is a decoder only; no MP3 encoder is bundled.
        (void)soundBuffer;
        std::cerr << "Mp3SoundBuffer::Write is not supported (dr_mp3 has no encoder): "
                  << file << std::endl;
    }
}
