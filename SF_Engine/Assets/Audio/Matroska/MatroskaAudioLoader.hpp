#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>

// 1st-party EBML/Matroska library
#include <1stPartyLibs/EBML/Element.hpp>
#include <1stPartyLibs/EBML/Schema.hpp>
#include <1stPartyLibs/Matroska/Block.hpp>
#include <1stPartyLibs/Matroska/MatroskaFile.hpp>
#include <1stPartyLibs/Matroska/MatroskaIds.hpp>
#include <1stPartyLibs/Matroska/MatroskaSchema.hpp>
#include <1stPartyLibs/Matroska/Streaming.hpp>
#include <1stPartyLibs/Matroska/Track.hpp>

#include <opus/opus.h>

namespace SF::Engine
{
    struct AudioTrackInfo
    {
        uint64_t trackNumber = 0;
        string codecID;
        string codecName;
        string language;
        double samplingFrequency = 48000.0;
        uint64_t channels        = 2;
        uint64_t bitDepth        = 0;
        vector<uint8_t> codecPrivate;
        bool enabled   = true;
        bool isDefault = false;
    };

    struct SegmentInfo
    {
        string title;
        string muxingApp;
        string writingApp;
        double duration        = 0.0;
        uint64_t timecodeScale = 1'000'000;
    };

    struct AudioFrame
    {
        uint64_t trackNumber = 0;
        uint64_t timestamp   = 0; // in segmentInfo.timecodeScale units, i.e. raw Matroska ticks
        vector<uint8_t> data;
        bool keyframe = true;
    };

    // Loads a Matroska/.mka file's audio tracks and frames using SF::EBML /
    // SF::Matroska. Unlike the original libmatroska-based version, this does
    // not lazily re-scan the file cluster by cluster: SF::Matroska::StreamingReader
    // parses an entire top-level master element (the whole Segment, including
    // every Cluster) into memory in one read_next_element() call - there's no
    // partial/skip-based walk available. Load() therefore parses the full
    // Segment once and extracts every audio frame from it up front into
    // allFrames_. For audio (as opposed to video) this is normally a
    // non-issue memory-wise, but it does mean Load() does the full pass
    // over the file rather than only reading headers immediately.
    class MatroskaAudioLoader
    {
    public:
        explicit MatroskaAudioLoader(string filePath);
        ~MatroskaAudioLoader();

        bool Load();
        void PrintInfo() const;

        const SegmentInfo &GetSegmentInfo() const { return segmentInfo; }
        const vector<AudioTrackInfo> &GetAudioTracks() const { return audioTracks; }

        bool ReadNextFrame(AudioFrame &frame);
        void SeekToTimestamp(uint64_t timestamp);

    private:
        string filePath;
        ifstream fileStream;

        SF::EBML::Schema schema{SF::Matroska::create_matroska_schema()};

        SegmentInfo segmentInfo;
        vector<AudioTrackInfo> audioTracks;

        vector<AudioFrame> allFrames_;
        size_t frameCursor_ = 0;

        bool loaded_ = false;

        void ParseInfo(const SF::EBML::Element &info);
        void ParseTracks(const SF::EBML::Element &tracks);
        void ParseTrackEntry(const SF::EBML::Element &entry);
        void ParseClusters(const SF::EBML::Element &segment);
        void ExtractFramesFromCluster(const SF::EBML::Element &cluster);

        bool IsAudioTrack(uint64_t trackNumber) const;

        void Reset();
    };

    struct OpusMkaOptions
    {
        int bitrate    = 0;  // bits/s; 0 = 64 kbps per channel
        int complexity = 10; // 0..10
        int frameMs    = 20; // 20 ms is exact at the default 1 ms timecode scale
    };

    namespace Detail
    {
        // Opus only accepts 8/12/16/24/48 kHz input.
        inline bool IsNativeOpusRate(uint32_t r)
        {
            return r == 8000 || r == 12000 || r == 16000 || r == 24000 || r == 48000;
        }

        // Linear-interpolation resampler. Adequate for editor export; swap in a proper
        // polyphase/sinc resampler if you care about the top octave.
        inline vector<int16_t> ResampleLinear(span<const int16_t> in, int channels, uint32_t from, uint32_t to)
        {
            const size_t inFrames = in.size() / channels;
            const auto outFrames  = static_cast<size_t>(llround(static_cast<double>(inFrames) * to / from));
            vector<int16_t> out(outFrames * channels);
            for (size_t i = 0; i < outFrames; ++i)
            {
                const double src = static_cast<double>(i) * from / to;
                const size_t i0  = min(static_cast<size_t>(src), inFrames - 1);
                const size_t i1  = min(i0 + 1, inFrames - 1);
                const double f   = src - static_cast<double>(i0);
                for (int c = 0; c < channels; ++c)
                {
                    const double a = in[i0 * channels + c], b = in[i1 * channels + c];
                    out[i * channels + c] = static_cast<int16_t>(lround(a + (b - a) * f));
                }
            }
            return out;
        }
    } // namespace Detail

    /**
     * @brief Encodes PCM to Opus and muxes it into a .mka.
     * @return true on success.
     */
    inline bool WriteOpusMka(span<const int16_t> pcmIn, uint32_t sampleRate, int channels, const filesystem::path &path,
                             const OpusMkaOptions &opt = {})
    {
        using namespace SF::Matroska;

        if (channels < 1 || channels > 2 || sampleRate == 0 || pcmIn.empty() || pcmIn.size() % channels != 0)
            return false;

        // 1. Bring the input to a rate Opus accepts.
        vector<int16_t> resampled;
        uint32_t rate           = sampleRate;
        span<const int16_t> pcm = pcmIn;
        if (!Detail::IsNativeOpusRate(rate))
        {
            resampled = Detail::ResampleLinear(pcmIn, channels, rate, 48000);
            rate      = 48000;
            pcm       = resampled;
        }
        const size_t totalFrames = pcm.size() / channels; // at `rate`

        // 2. Encoder.
        int err = 0;
        unique_ptr<OpusEncoder, decltype(&opus_encoder_destroy)> enc(
                opus_encoder_create(static_cast<opus_int32>(rate), channels, OPUS_APPLICATION_AUDIO, &err),
                &opus_encoder_destroy);
        if (!enc || err != OPUS_OK)
            return false;
        opus_encoder_ctl(enc.get(), OPUS_SET_BITRATE(opt.bitrate > 0 ? opt.bitrate : 64000 * channels));
        opus_encoder_ctl(enc.get(), OPUS_SET_COMPLEXITY(opt.complexity));

        opus_int32 lookahead = 0; // samples at `rate`
        opus_encoder_ctl(enc.get(), OPUS_GET_LOOKAHEAD(&lookahead));

        const auto preSkip48   = static_cast<uint32_t>(static_cast<uint64_t>(lookahead) * 48000 / rate);
        const size_t frameSize = static_cast<size_t>(rate) * opt.frameMs / 1000; // per channel, at `rate`
        const size_t frame48   = static_cast<size_t>(48000) * opt.frameMs / 1000;

        ofstream out(path, ios::binary | ios::trunc);
        if (!out)
            return false;

        MatroskaFile file;

        // OpusHead: magic, version, channels, preskip (LE16), input rate (LE32), gain (LE16), mapping family
        vector<EBML::byte> head = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, static_cast<EBML::byte>(channels)};
        auto le16               = [&](uint32_t v)
        {
            head.push_back(v & 0xFF);
            head.push_back((v >> 8) & 0xFF);
        };
        auto le32 = [&](uint32_t v)
        {
            le16(v & 0xFFFF);
            le16(v >> 16);
        };
        le16(preSkip48);
        le32(sampleRate);  // informational: the original rate, not necessarily what we encoded at
        le16(0);           // output gain
        head.push_back(0); // mapping family 0 (mono/stereo)

        MatroskaFile::AudioTrackOptions topt;
        topt.codecPrivate = head;
        topt.codecDelay =
                chrono::nanoseconds((static_cast<uint64_t>(preSkip48) * 1'000'000'000ull + 47999) / 48000); // round up
        topt.seekPreRoll     = chrono::milliseconds(80);
        topt.defaultDuration = chrono::milliseconds(opt.frameMs);
        const Track track    = file.add_audio_track(48000.0, channels, "A_OPUS", topt);

        file.initialize_writer(out);

        const size_t fedFrames   = totalFrames + static_cast<size_t>(lookahead);
        const size_t packetCount = (fedFrames + frameSize - 1) / frameSize;

        vector<int16_t> frameBuf(frameSize * channels);
        vector<EBML::byte> packet(4000);

        for (size_t p = 0; p < packetCount; ++p)
        {
            ranges::fill(frameBuf, 0);
            if (const size_t start = p * frameSize; start < totalFrames)
            {
                const size_t n = min(frameSize, totalFrames - start);
                copy_n(pcm.begin() + static_cast<long>(start) * channels, n * channels, frameBuf.begin());
            }

            const int bytes = opus_encode(enc.get(), frameBuf.data(), static_cast<int>(frameSize), packet.data(),
                                          static_cast<opus_int32>(packet.size()));
            if (bytes < 0)
                return false;

            const int64_t ts = int64_t(p) * opt.frameMs; // 1 tick = 1 ms
            const bool last  = (p + 1 == packetCount);

            int64_t discardNs = 0;
            if (last)
            {
                // Samples (at 48 kHz) the decoder will produce beyond the real audio.
                const uint64_t decoded48 = static_cast<uint64_t>(packetCount) * frame48;
                const uint64_t real48    = static_cast<uint64_t>(totalFrames) * 48000 / rate;
                const uint64_t trim48    = decoded48 - preSkip48 - min<uint64_t>(real48, decoded48 - preSkip48);
                // Round UP: decoders floor ns->samples, so truncating here would leave 1 sample too many.
                discardNs = static_cast<int64_t>((trim48 * 1'000'000'000ull + 47999) / 48000);
            }

            file.write_frame(track.number, ts, span<const EBML::byte>(packet.data(), static_cast<size_t>(bytes)), true,
                             last ? static_cast<uint64_t>(opt.frameMs) : 0, discardNs);
        }

        file.finalize();
        out.flush();
        return static_cast<bool>(out);
    }
} // namespace SF::Engine
