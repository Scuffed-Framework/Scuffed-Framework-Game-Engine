#pragma once

#include "../EBML/Element.hpp"
#include "MatroskaIds.hpp"
#include "Block.hpp" 
#include <string>
#include <optional>
#include <variant>
#include <vector>
#include <memory>
#include <chrono>

using namespace SF::EBML;
namespace SF::Matroska
{
    enum class TrackType : uint64_t
    {
        Video = 1,
        Audio = 2,
        Complex = 3,
        Logo = 0x10,
        Subtitle = 0x11,
        Buttons = 0x12,
        Control = 0x20,
        Metadata = 0x21
    };

    // Video track specific data
    struct VideoTrack
    {
        uint64_t pixelWidth = 0;
        uint64_t pixelHeight = 0;
        uint64_t pixelCropTop = 0;
        uint64_t pixelCropBottom = 0;
        uint64_t pixelCropLeft = 0;
        uint64_t pixelCropRight = 0;
        double frameRate = 0.0;
        uint64_t displayWidth = 0;
        uint64_t displayHeight = 0;
        uint64_t displayUnit = 0;
        bool interlaced = false;
        bool stereoMode = false;
        std::vector<byte> codecPrivate;
        std::string codecID;
        
        bool validate() const
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
        double samplingFrequency = 8000.0;
        uint64_t outputSamplingFrequency = 0;
        uint64_t channels = 1;
        uint64_t bitDepth = 0;
        std::vector<byte> codecPrivate;
        std::string codecID;
        
        bool validate() const
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
        std::string codecID;
        bool forced = false;
        bool defaultTrack = true;
        std::vector<byte> codecPrivate;
    };

    // Enhanced Track with validation
    struct Track
    {
        uint64_t number = 0;
        uint64_t uid = 0;
        TrackType type = TrackType::Video;
        std::string name;
        std::string language = "eng";
        std::string codecID;
        bool enabled = true;
        bool defaultTrack = true;
        bool forced = false;
        bool lacing = true;
        std::chrono::nanoseconds defaultDuration = std::chrono::nanoseconds::zero();
        std::chrono::nanoseconds codecDelay = std::chrono::nanoseconds::zero();
        std::chrono::nanoseconds seekPreRoll = std::chrono::nanoseconds::zero();

        std::variant<VideoTrack, AudioTrack, SubtitleTrack, std::monostate> specific;

        bool validate() const
        {
            if (number == 0 || uid == 0)
                return false;
            if (codecID.empty())
                return false;
            
            if (type == TrackType::Video)
            {
                if (auto* v = std::get_if<VideoTrack>(&specific))
                    return v->validate();
            }
            else if (type == TrackType::Audio)
            {
                if (auto* a = std::get_if<AudioTrack>(&specific))
                    return a->validate();
            }
            
            return true;
        }
    };

    // Build a TrackEntry element from a Track. Shared by MatroskaFile and
    // MatroskaMuxer so both can serialize tracks the same way.
    inline Element create_track_entry(const Track& track)
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
            entry.add(Element::make_uint(ids::DefaultDuration,
                         static_cast<uint64_t>(track.defaultDuration.count())));
        if (track.codecDelay.count() > 0)
            entry.add(Element::make_uint(ids::CodecDelay,
                         static_cast<uint64_t>(track.codecDelay.count())));
        if (track.seekPreRoll.count() > 0)
            entry.add(Element::make_uint(ids::SeekPreRoll,
                         static_cast<uint64_t>(track.seekPreRoll.count())));

        if (track.type == TrackType::Video)
        {
            if (auto* video = std::get_if<VideoTrack>(&track.specific))
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
        }
        else if (track.type == TrackType::Audio)
        {
            if (auto* audio = std::get_if<AudioTrack>(&track.specific))
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
        std::vector<int64_t> referenceBlocks;
        uint64_t duration = 0;
        uint64_t codecState = 0;
        std::vector<byte> discardPadding;
        ElementList additionalData;
        
        bool is_b_frame() const
        {
            for (auto ref : referenceBlocks)
                if (ref < 0)
                    return true;
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
        std::shared_ptr<std::vector<byte>> data;
        int64_t duration = 0;
        
        SharedFrame() = default;
        SharedFrame(const std::vector<byte>& d) 
            : data(std::make_shared<std::vector<byte>>(d)) {}
        SharedFrame(std::vector<byte>&& d) 
            : data(std::make_shared<std::vector<byte>>(std::move(d))) {}
        SharedFrame(const SharedFrame&) = default;
        SharedFrame(SharedFrame&&) = default;
        
        SharedFrame& operator=(const SharedFrame&) = default;
        SharedFrame& operator=(SharedFrame&&) = default;
        
        const std::vector<byte>& get() const { return *data; }
        size_t size() const { return data ? data->size() : 0; }
        bool empty() const { return !data || data->empty(); }
    };

    // Enhanced Block with shared frames
    struct EnhancedBlock
    {
        uint64_t trackNumber = 0;
        int16_t timecode = 0;
        bool keyframe = true;
        bool invisible = false;
        bool discardable = false;
        LacingType lacing = LacingType::None;
        std::vector<SharedFrame> frames;
        
        void add_frame(std::vector<byte>&& data)
        {
            frames.emplace_back(std::move(data));
        }
        
        void add_frame(const std::vector<byte>& data)
        {
            frames.emplace_back(data);
        }
        
        void add_frame(std::shared_ptr<std::vector<byte>> data)
        {
            SharedFrame frame;
            frame.data = std::move(data);
            frames.push_back(std::move(frame));
        }
        
        bool validate() const
        {
            if (trackNumber == 0 || frames.empty())
                return false;
            if (lacing != LacingType::None && frames.size() < 2)
                return false;
            return true;
        }
    };
}