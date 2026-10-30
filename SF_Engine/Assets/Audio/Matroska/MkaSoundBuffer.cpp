#include "MkaSoundBuffer.hpp"
#include <iostream>
#include <memory>
#include <opus/opus.h>
#include <vector>
#include "MatroskaAudioLoader.hpp"

namespace SF::Engine
{
    namespace
    {
        struct OpusDecoderDeleter
        {
            void operator()(OpusDecoder *d) const { opus_decoder_destroy(d); }
        };
    } // namespace

    void MkaSoundBuffer::Load(SoundBuffer &soundBuffer, const DataInput &input)
    {
        MatroskaAudioLoader loader(input.file->GetFullPath());
        if (!loader.Load())
        {
            std::cerr << "Failed to parse MKA file: " << input.file->GetFullPath() << std::endl;
            return;
        }

        // First enabled Opus track
        const AudioTrackInfo *track = nullptr;
        for (const auto &t: loader.GetAudioTracks())
            if (t.enabled && t.codecID == "A_OPUS")
            {
                track = &t;
                break;
            }

        if (!track)
        {
            std::cerr << "MKA has no A_OPUS track (other codecs not supported yet)" << std::endl;
            return;
        }

        // OpusHead: "OpusHead"(8) ver(1) channels(1) preSkip(2 LE) inputRate(4) gain(2) mappingFamily(1)
        const auto &head = track->codecPrivate;
        if (head.size() < 19 || std::memcmp(head.data(), "OpusHead", 8) != 0)
        {
            std::cerr << "MKA Opus track has invalid CodecPrivate" << std::endl;
            return;
        }

        const int headChannels = head[9];
        const size_t preSkip   = head[10] | (head[11] << 8);
        const int mapping      = head[18];

        if (mapping != 0 || headChannels < 1 || headChannels > 2)
        {
            std::cerr << "Unsupported MKA Opus layout (mapping " << mapping << ", " << headChannels << "ch)"
                      << std::endl;
            return;
        }

        int err = 0;
        std::unique_ptr<OpusDecoder, OpusDecoderDeleter> dec(opus_decoder_create(48000, headChannels, &err));
        if (!dec)
        {
            std::cerr << "opus_decoder_create failed: " << err << std::endl;
            return;
        }

        std::vector<int16_t> pcm;
        std::vector<int16_t> chunk(5760 * headChannels); // 120 ms max per packet

        AudioFrame frame;
        while (loader.ReadNextFrame(frame))
        {
            if (frame.trackNumber != track->trackNumber)
                continue;

            const int n = opus_decode(dec.get(), frame.data.data(), static_cast<opus_int32>(frame.data.size()),
                                      chunk.data(), 5760, 0);
            if (n < 0)
            {
                std::cerr << "Opus decode error " << n << std::endl;
                continue; // skip a bad packet
            }
            pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(n) * headChannels);
        }

        // Pre-skip = encoder delay samples to discard from the start
        const size_t skipValues = preSkip * headChannels;
        if (pcm.size() > skipValues)
            pcm.erase(pcm.begin(), pcm.begin() + skipValues);

        if (pcm.empty())
        {
            std::cerr << "MKA decoded to zero samples" << std::endl;
            return;
        }

        soundBuffer.SetPCM(
                {.Samples = std::move(pcm), .SampleRate = 48000, .ChannelCount = static_cast<uint8_t>(headChannels)});
    }

    void MkaSoundBuffer::Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file)
    {
        if (!soundBuffer.HasPCM())
        {
            std::cerr << "MkaSoundBuffer::Write: buffer has no PCM (was ReleasePCM() called?): " << file << std::endl;
            return;
        }
 
        const auto &pcm = soundBuffer.GetPCM();
        if (!WriteOpusMka(pcm.Samples, pcm.SampleRate, pcm.ChannelCount, file))
            std::cerr << "MkaSoundBuffer::Write: failed to write " << file << std::endl;
    }
} // namespace SF::Engine
