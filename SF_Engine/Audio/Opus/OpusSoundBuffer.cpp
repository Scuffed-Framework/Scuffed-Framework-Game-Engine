#include "OpusSoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif

#define DR_WAV_IMPLEMENTATION
#include "dr_opus.h"

#include <iostream>
#include <vector>

namespace SF::Engine
{
    void OpusSoundBuffer::Load(SoundBuffer &soundBuffer, const std::filesystem::path &filename)
    {
        dropus opus;
        if (!dropus_init_file(&opus, filename.string().c_str(), nullptr))
        {
            std::cerr << "Failed to open WAV file: " << filename << std::endl;
            return;
        }

        // TODO: IMPL

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to buffer WAV data for " << filename << ": " << error << std::endl;
        }

        dropus_uninit(&opus);
    }

    void OpusSoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &filename)
    {
        // NOTE: Core OpenAL has no alGetBufferData equivalent, so PCM samples can't be
        // read back out of an AL buffer for re-encoding. Writing requires the decoded
        // PCM to be cached somewhere accessible (e.g. on SoundBuffer itself) before this
        // can be implemented for real. Left as a stub so the extension is still registered.
        (void)soundBuffer;
        std::cerr << "OpusSoundBuffer::Write is not implemented (no PCM readback from OpenAL buffer): "
                  << filename << std::endl;
    }
}