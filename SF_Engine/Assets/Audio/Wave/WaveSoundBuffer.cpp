#include "WaveSoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.hpp"

#include <iostream>
#include <vector>

namespace SF::Engine
{
    void WaveSoundBuffer::Load(SoundBuffer &soundBuffer, const DataInput &input)
    {
        drwav wav;
        if (!drwav_init_file(&wav, input.file->GetName().c_str(), nullptr))
        {
            std::cerr << "Failed to open WAV file: " << input.file << std::endl;
            return;
        }

        if (wav.channels < 1 || wav.channels > 2)
        {
            std::cerr << "Unsupported WAV channel count (" << wav.channels
                       << ") in " << input.file << ", only mono/stereo supported" << std::endl;
            drwav_uninit(&wav);
            return;
        }

        std::vector<int16_t> pcm(static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels);
        drwav_uint64 framesRead = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcm.data());

        ALenum format = (wav.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        alBufferData(soundBuffer.GetBuffer(), format, pcm.data(),
                     static_cast<ALsizei>(framesRead * wav.channels * sizeof(int16_t)),
                     static_cast<ALsizei>(wav.sampleRate));

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to buffer WAV data for " << input.file << ": " << error << std::endl;
        }

        drwav_uninit(&wav);
    }

    void WaveSoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file)
    {
        // NOTE: Core OpenAL has no alGetBufferData equivalent, so PCM samples can't be
        // read back out of an AL buffer for re-encoding. Writing requires the decoded
        // PCM to be cached somewhere accessible (e.g. on SoundBuffer itself) before this
        // can be implemented for real. Left as a stub so the extension is still registered.
        (void)soundBuffer;
        std::cerr << "WaveSoundBuffer::Write is not implemented (no PCM readback from OpenAL buffer): "
                  << file << std::endl;
    }
}