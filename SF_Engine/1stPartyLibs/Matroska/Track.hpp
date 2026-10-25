#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "../EBML/Element.hpp"
#include "Block.hpp"
#include "MatroskaIds.hpp"

using namespace SF::EBML;
namespace SF::Matroska
{
    using namespace std;
    enum class TrackType : uint8_t
    {
        Video    = 1,
        Audio    = 2,
        Complex  = 3,
        Logo     = 0x10,
        Subtitle = 0x11,
        Buttons  = 0x12,
        Control  = 0x20,
        Metadata = 0x21
    };

    // Video track specific data
    struct VideoTrack
    {
        uint64_t pixelWidth      = 0;
        uint64_t pixelHeight     = 0;
        uint64_t pixelCropTop    = 0;
        uint64_t pixelCropBottom = 0;
        uint64_t pixelCropLeft   = 0;
        uint64_t pixelCropRight  = 0;
        double frameRate         = 0.0;
        uint64_t displayWidth    = 0;
        uint64_t displayHeight   = 0;
        uint64_t displayUnit     = 0;
        bool interlaced          = false;
        bool stereoMode          = false;
        vector<EBML::byte> codecPrivate;
        string codecID;

        [[nodiscard]] bool validate() const
        {
            if (pixelWidth == 0 || pixelHeight == 0)
                return false;
            if (codecID.empty())
                return false;
            return true;
        }
    };

    struct AudioTrack
    {
        double samplingFrequency         = 8000.0;
        uint64_t outputSamplingFrequency = 0;
        uint64_t channels                = 1;
        uint64_t bitDepth                = 0;
        vector<EBML::byte> codecPrivate;
        string codecID;

        [[nodiscard]] bool validate() const
        {
            if (samplingFrequency <= 0 || channels == 0)
                return false;
            if (codecID.empty())
                return false;
            return true;
        }
    };

    struct SubtitleTrack
    {
        string codecID;
        bool forced       = false;
        bool defaultTrack = true;
        vector<EBML::byte> codecPrivate;
    };

    // Enhanced Track with validation
    struct Track
    {
        uint64_t number = 0;
        uint64_t uid    = 0;
        TrackType type  = TrackType::Video;
        string name;
        string language = "eng";
        string codecID;
        bool enabled                        = true;
        bool defaultTrack                   = true;
        bool forced                         = false;
        bool lacing                         = true;
        chrono::nanoseconds defaultDuration = chrono::nanoseconds::zero();
        chrono::nanoseconds codecDelay      = chrono::nanoseconds::zero();
        chrono::nanoseconds seekPreRoll     = chrono::nanoseconds::zero();

        variant<VideoTrack, AudioTrack, SubtitleTrack, monostate> specific;

        [[nodiscard]] bool validate() const
        {
            if (number == 0 || uid == 0)
                return false;
            if (codecID.empty())
                return false;

            if (type == TrackType::Video)
            {
                if (auto *v = get_if<VideoTrack>(&specific))
                    return v->validate();
            } else if (type == TrackType::Audio)
            {
                if (auto *a = get_if<AudioTrack>(&specific))
                    return a->validate();
            }

            return true;
        }
    };

    // Build a TrackEntry element from a Track. Shared by MatroskaFile and
    // MatroskaMuxer so both can serialize tracks the same way.
    inline Element create_track_entry(const Track &track)
    {
        auto entry = Element::make_master(ids::TrackEntry);

        entry.add(Element::make_uint(ids::TrackNumber, track.number));
        entry.add(Element::make_uint(ids::TrackUID, track.uid));
        entry.add(Element::make_uint(ids::TrackType, static_cast<uint64_t>(track.type)));
        entry.add(Element::make_utf8(ids::CodecID, track.codecID));

        if (!track.name.empty())
            entry.add(Element::make_utf8(ids::Name, track.name));
        if (track.language != "eng")
            entry.add(Element::make_string(ids::Language, track.language));
        if (!track.enabled)
            entry.add(Element::make_uint(ids::TrackEnabled, 0));
        if (!track.defaultTrack)
            entry.add(Element::make_uint(ids::TrackDefault, 0));
        if (track.forced)
            entry.add(Element::make_uint(ids::TrackForced, 1));
        if (!track.lacing)
            entry.add(Element::make_uint(ids::TrackLacing, 0));
        if (track.defaultDuration.count() > 0)
            entry.add(Element::make_uint(ids::DefaultDuration, static_cast<uint64_t>(track.defaultDuration.count())));
        if (track.codecDelay.count() > 0)
            entry.add(Element::make_uint(ids::CodecDelay, static_cast<uint64_t>(track.codecDelay.count())));
        if (track.seekPreRoll.count() > 0)
            entry.add(Element::make_uint(ids::SeekPreRoll, static_cast<uint64_t>(track.seekPreRoll.count())));

        if (track.type == TrackType::Video)
        {
            if (auto *video = get_if<VideoTrack>(&track.specific))
            {
                if (!video->codecPrivate.empty())
                    entry.add(Element::make_binary(ids::CodecPrivate, video->codecPrivate));

                // Per spec (and the schema this project registers), video-
                // specific fields belong inside a nested Video master, not
                // flat under TrackEntry.
                auto videoElement = Element::make_master(ids::Video);
                videoElement.add(Element::make_uint(ids::PixelWidth, video->pixelWidth));
                videoElement.add(Element::make_uint(ids::PixelHeight, video->pixelHeight));
                if (video->pixelCropTop)
                    videoElement.add(Element::make_uint(ids::PixelCropTop, video->pixelCropTop));
                if (video->pixelCropBottom)
                    videoElement.add(Element::make_uint(ids::PixelCropBottom, video->pixelCropBottom));
                if (video->pixelCropLeft)
                    videoElement.add(Element::make_uint(ids::PixelCropLeft, video->pixelCropLeft));
                if (video->pixelCropRight)
                    videoElement.add(Element::make_uint(ids::PixelCropRight, video->pixelCropRight));
                if (video->frameRate > 0.0)
                    videoElement.add(Element::make_float(ids::FrameRate, video->frameRate));
                if (video->displayWidth)
                    videoElement.add(Element::make_uint(ids::DisplayWidth, video->displayWidth));
                if (video->displayHeight)
                    videoElement.add(Element::make_uint(ids::DisplayHeight, video->displayHeight));
                if (video->displayUnit)
                    videoElement.add(Element::make_uint(ids::DisplayUnit, video->displayUnit));
                if (video->interlaced)
                    videoElement.add(Element::make_uint(ids::FlagInterlaced, 1));
                if (video->stereoMode)
                    videoElement.add(Element::make_uint(ids::StereoMode, 1));
                entry.add(std::move(videoElement));
            }
        } else if (track.type == TrackType::Audio)
        {
            if (auto *audio = get_if<AudioTrack>(&track.specific))
            {
                if (!audio->codecPrivate.empty())
                    entry.add(Element::make_binary(ids::CodecPrivate, audio->codecPrivate));

                // Per spec, audio-specific fields belong inside a nested
                // Audio master, not flat under TrackEntry.
                auto audioElement = Element::make_master(ids::Audio);
                audioElement.add(Element::make_float(ids::SamplingFrequency, audio->samplingFrequency));
                if (audio->outputSamplingFrequency)
                    audioElement.add(Element::make_uint(ids::OutputSamplingFrequency, audio->outputSamplingFrequency));
                audioElement.add(Element::make_uint(ids::Channels, audio->channels));
                if (audio->bitDepth)
                    audioElement.add(Element::make_uint(ids::BitDepth, audio->bitDepth));
                entry.add(std::move(audioElement));
            }
        }

        return entry;
    }

    // BlockGroup support for B-frames
    struct BlockGroup
    {
        Block block;
        vector<int64_t> referenceBlocks;
        uint64_t duration   = 0;
        uint64_t codecState = 0;
        vector<EBML::byte> discardPadding;
        ElementList additionalData;

        bool is_b_frame() const
        {
            if (ranges::any_of(referenceBlocks, [](const auto &ref) { return ref < 0; }))
            {
                return true;
            }
            return false;
        }

        bool validate() const
        {
            if (block.frames.empty())
                return false;
            if (block.trackNumber == 0)
                return false;
            return true;
        }
    };

    // Frame with reference counting for better memory management
    struct SharedFrame
    {
        shared_ptr<vector<EBML::byte>> data;
        int64_t duration = 0;

        SharedFrame() = default;
        explicit SharedFrame(const vector<EBML::byte> &d) : data(make_shared<vector<EBML::byte>>(d)) {}
        explicit SharedFrame(vector<EBML::byte> &&d) : data(make_shared<vector<EBML::byte>>(std::move(d))) {}
        SharedFrame(const SharedFrame &) = default;
        SharedFrame(SharedFrame &&)      = default;

        SharedFrame &operator=(const SharedFrame &) = default;
        SharedFrame &operator=(SharedFrame &&)      = default;

        [[nodiscard]] const vector<EBML::byte> &get() const { return *data; }
        [[nodiscard]] size_t size() const { return data ? data->size() : 0; }
        [[nodiscard]] bool empty() const { return !data || data->empty(); }
    };

    // Enhanced Block with shared frames
    struct EnhancedBlock
    {
        uint64_t trackNumber = 0;
        int16_t timecode     = 0;
        bool keyframe        = true;
        bool invisible       = false;
        bool discardable     = false;
        LacingType lacing    = LacingType::None;
        vector<SharedFrame> frames;

        void add_frame(vector<EBML::byte> &&data) { frames.emplace_back(std::move(data)); }

        void add_frame(const vector<EBML::byte> &data) { frames.emplace_back(data); }

        void add_frame(shared_ptr<vector<EBML::byte>> data)
        {
            SharedFrame frame;
            frame.data = std::move(data);
            frames.push_back(std::move(frame));
        }

        [[nodiscard]] bool validate() const
        {
            if (trackNumber == 0 || frames.empty())
                return false;
            if (lacing != LacingType::None && frames.size() < 2)
                return false;
            return true;
        }
    };
} // namespace SF::Matroska
