#include "OpusSoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
    #include <OpenAL/al.h>
#else
    #include <al.h>
#endif

#include <opus/opusfile.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace SF::Engine
{
    namespace
    {
        struct OpusFileDeleter
        {
            void operator()(OggOpusFile *f) const { op_free(f); }
        };
    } // namespace

    void OpusSoundBuffer::Load(SoundBuffer &soundBuffer, const DataInput &input)
    {
        // Works whether GetFullPath() returns std::string or std::filesystem::path.
        const std::string path = std::filesystem::path(input.file->GetFullPath()).string();

        int err = 0;
        std::unique_ptr<OggOpusFile, OpusFileDeleter> opus(op_open_file(path.c_str(), &err));
        if (!opus)
        {
            std::cerr << "Failed to open Opus file: " << path << " (opusfile error " << err << ")" << std::endl;
            return;
        }

        const int channels = op_channel_count(opus.get(), 0);
        // OpenAL only spatializes mono sources, so keep mono as mono. Anything else is
        // downmixed to stereo by op_read_stereo (core AL has no >2 channel formats).
        const bool mono       = (channels == 1);
        const int outChannels = mono ? 1 : 2;
        const ALenum format   = mono ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        std::vector<opus_int16> pcm;
        const ogg_int64_t totalFrames = op_pcm_total(opus.get(), -1); // per channel, 48 kHz; <0 if unknown
        if (totalFrames > 0)
            pcm.reserve(static_cast<size_t>(totalFrames) * outChannels);

        // op_read* return frames per channel, and want the buffer size in total int16 values.
        // 5760 frames is 120 ms, the largest Opus packet.
        std::vector<opus_int16> chunk(5760 * outChannels);
        while (true)
        {
            const int frames = mono ? op_read(opus.get(), chunk.data(), static_cast<int>(chunk.size()), nullptr)
                                    : op_read_stereo(opus.get(), chunk.data(), static_cast<int>(chunk.size()));
            if (frames == 0)
                break; // EOF
            if (frames < 0)
            {
                // OP_HOLE means a gap in the data, which is recoverable; anything else is fatal.
                if (frames == OP_HOLE)
                    continue;
                std::cerr << "Opus decode error " << frames << " in " << path << std::endl;
                return;
            }
            pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(frames) * outChannels);
        }

        if (pcm.empty())
        {
            std::cerr << "Opus file decoded to zero samples: " << path << std::endl;
            return;
        }

        alGetError(); // clear any stale error so we only report ours
        alBufferData(soundBuffer.GetBuffer(), format, pcm.data(), static_cast<ALsizei>(pcm.size() * sizeof(opus_int16)),
                     48000);

        if (const ALenum error = alGetError(); error != AL_NO_ERROR)
            std::cerr << "Failed to buffer Opus data for " << path << ": " << error << std::endl;
    }
} // namespace SF::Engine
