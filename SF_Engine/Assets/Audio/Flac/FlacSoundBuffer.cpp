#include "FlacSoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.hpp"

#include <iostream>
#include <vector>

namespace SF::Engine
{
    void FlacSoundBuffer::Load(SoundBuffer &soundBuffer, const std::filesystem::path &filename)
    {
        drflac *flac = drflac_open_file(filename.string().c_str(), nullptr);
        if (!flac)
        {
            std::cerr << "Failed to open FLAC file: " << filename << std::endl;
            return;
        }

        if (flac->channels < 1 || flac->channels > 2)
        {
            std::cerr << "Unsupported FLAC channel count (" << flac->channels
                       << ") in " << filename << ", only mono/stereo supported" << std::endl;
            drflac_close(flac);
            return;
        }

        std::vector<int16_t> pcm(static_cast<size_t>(flac->totalPCMFrameCount) * flac->channels);
        drflac_uint64 framesRead = drflac_read_pcm_frames_s16(flac, flac->totalPCMFrameCount, pcm.data());

        ALenum format = (flac->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        alBufferData(soundBuffer.GetBuffer(), format, pcm.data(),
                     static_cast<ALsizei>(framesRead * flac->channels * sizeof(int16_t)),
                     static_cast<ALsizei>(flac->sampleRate));

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to buffer FLAC data for " << filename << ": " << error << std::endl;
        }

        drflac_close(flac);
    }

    void FlacSoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &filename)
    {
        // dr_flac is a decoder only; no FLAC encoder is bundled.
        (void)soundBuffer;
        std::cerr << "FlacSoundBuffer::Write is not supported (dr_flac has no encoder): "
                  << filename << std::endl;
    }
}
