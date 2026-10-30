#include "MatroskaAudioLoader.hpp"
#include <algorithm>
#include <iomanip>
#include <utility>

using namespace SF::EBML;
using namespace SF::Matroska;

namespace SF::Engine
{
    MatroskaAudioLoader::MatroskaAudioLoader(std::string filePath) : filePath(std::move(filePath)) {}

    MatroskaAudioLoader::~MatroskaAudioLoader() { Reset(); }

    void MatroskaAudioLoader::Reset()
    {
        allFrames_.clear();
        frameCursor_ = 0;
        audioTracks.clear();
        segmentInfo = SegmentInfo{};
        loaded_     = false;
        if (fileStream.is_open())
            fileStream.close();
    }

    bool MatroskaAudioLoader::Load()
    {
        Reset();

        fileStream.open(filePath, std::ios::binary);
        if (!fileStream)
            return false;

        try
        {
            StreamingReader reader(fileStream, schema);

            if (const auto headerElement = reader.read_next_element();
                !headerElement || headerElement->id() != ::SF::EBML::ids::EBML)
                return false;

            const auto segmentOpt = reader.read_next_element();
            if (!segmentOpt || segmentOpt->id() != ::SF::Matroska::ids::Segment)
                return false;

            const Element &segment = *segmentOpt;

            for (const auto &child: segment.children())
            {
                if (child.id() == ::SF::Matroska::ids::Info)
                    ParseInfo(child);
                else if (child.id() == ::SF::Matroska::ids::Tracks)
                    ParseTracks(child);
            }

            // Tracks must be known before we can filter Cluster frames by
            // audio-track membership, so this runs after the loop above.
            ParseClusters(segment);

            loaded_ = true;
            return true;
        } catch (const std::exception &)
        {
            Reset();
            return false;
        }
    }

    void MatroskaAudioLoader::ParseInfo(const Element &info)
    {
        if (auto *e = info.find(::SF::Matroska::ids::TimecodeScale))
            segmentInfo.timecodeScale = e->as_uint();
        if (auto *e = info.find(::SF::Matroska::ids::Duration))
            segmentInfo.duration = e->as_float();
        if (auto *e = info.find(::SF::Matroska::ids::Title))
            segmentInfo.title = e->as_string();
        if (auto *e = info.find(::SF::Matroska::ids::MuxingApp))
            segmentInfo.muxingApp = e->as_string();
        if (auto *e = info.find(::SF::Matroska::ids::WritingApp))
            segmentInfo.writingApp = e->as_string();
    }

    void MatroskaAudioLoader::ParseTracks(const Element &tracks)
    {
        for (const auto &entry: tracks.children())
            if (entry.id() == ::SF::Matroska::ids::TrackEntry)
                ParseTrackEntry(entry);
    }

    void MatroskaAudioLoader::ParseTrackEntry(const Element &entry)
    {
        AudioTrackInfo info;
        bool isAudioTrack = false;

        if (auto *e = entry.find(::SF::Matroska::ids::TrackNumber))
            info.trackNumber = e->as_uint();
        if (auto *e = entry.find(::SF::Matroska::ids::TrackType))
            isAudioTrack = (e->as_uint() == static_cast<uint64_t>(TrackType::Audio));
        if (auto *e = entry.find(::SF::Matroska::ids::CodecID))
            info.codecID = e->as_string();
        if (auto *e = entry.find(::SF::Matroska::ids::CodecName))
            info.codecName = e->as_string();
        if (auto *e = entry.find(::SF::Matroska::ids::Language))
            info.language = e->as_string();
        if (auto *e = entry.find(::SF::Matroska::ids::CodecPrivate))
            info.codecPrivate = e->raw();
        if (auto *e = entry.find(::SF::Matroska::ids::TrackEnabled))
            info.enabled = e->as_uint() != 0;
        if (auto *e = entry.find(::SF::Matroska::ids::TrackDefault))
            info.isDefault = e->as_uint() != 0;

        if (auto *audio = entry.find(::SF::Matroska::ids::Audio))
        {
            if (auto *e = audio->find(::SF::Matroska::ids::SamplingFrequency))
                info.samplingFrequency = e->as_float();
            if (auto *e = audio->find(::SF::Matroska::ids::Channels))
                info.channels = e->as_uint();
            if (auto *e = audio->find(::SF::Matroska::ids::BitDepth))
                info.bitDepth = e->as_uint();
        }

        if (isAudioTrack)
            audioTracks.push_back(std::move(info));
    }

    bool MatroskaAudioLoader::IsAudioTrack(uint64_t trackNumber) const
    {
        return ranges::any_of(audioTracks, [trackNumber](const auto &t) { return t.trackNumber == trackNumber; });
    }

    void MatroskaAudioLoader::ParseClusters(const Element &segment)
    {
        for (const auto &child: segment.children())
            if (child.id() == ::SF::Matroska::ids::Cluster)
                ExtractFramesFromCluster(child);
    }

    void MatroskaAudioLoader::ExtractFramesFromCluster(const Element &cluster)
    {
        uint64_t clusterTimecode = 0;
        if (auto *tc = cluster.find(::SF::Matroska::ids::Timecode))
            clusterTimecode = tc->as_uint();

        auto resolveTimestamp = [clusterTimecode](const int16_t blockTimecode) -> uint64_t
        {
            const int64_t ts = static_cast<int64_t>(clusterTimecode) + blockTimecode;
            return static_cast<uint64_t>(std::max<int64_t>(ts, 0));
        };

        // NOTE: when a block is laced (multiple frames packed into one SimpleBlock/Block), every sub-frame is stamped
        // with the block's single timestamp here. Matroska doesn't carry a per-laced-frame duration, and
        // Block::Frame::duration is never populated by decode_simple_block(). If you need exact per-frame timestamps
        // for a laced codec (Vorbis/Opus frequently lace), derive per-frame duration from the codec itself (e.g. each
        // Opus packet's TOC byte, or a fixed sample count for a given Vorbis block size) and offset from this block's
        // timestamp accordingly.
        for (const auto &child: cluster.children())
        {
            if (child.id() == ::SF::Matroska::ids::SimpleBlock)
            {
                SimpleBlock block;
                try
                {
                    block = decode_simple_block(child.raw(), true);
                } catch (const std::exception &)
                {
                    continue;
                }

                if (!IsAudioTrack(block.trackNumber))
                    continue;

                const uint64_t timestamp = resolveTimestamp(block.timecode);
                for (auto &[data, duration]: block.frames)
                {
                    AudioFrame af;
                    af.trackNumber = block.trackNumber;
                    af.timestamp   = timestamp;
                    af.keyframe    = block.keyframe;
                    af.data.assign(data.begin(), data.end());
                    allFrames_.push_back(std::move(af));
                }
            } else if (child.id() == ::SF::Matroska::ids::BlockGroup)
            {
                const Element *blockElem = child.find(::SF::Matroska::ids::Block);
                if (!blockElem)
                    continue;

                SimpleBlock block;
                try
                {
                    block = decode_simple_block(blockElem->raw(), false);
                } catch (const std::exception &)
                {
                    continue;
                }

                if (!IsAudioTrack(block.trackNumber))
                    continue;

                // Standard Matroska convention: a block with no ReferenceBlock
                // child depends on nothing else, i.e. it's a keyframe. (The
                // flags byte inside a plain Block as opposed to a
                // SimpleBlock - carries no meaningful keyframe bit of its own.)
                const bool isKeyframe    = (child.find(::SF::Matroska::ids::ReferenceBlock) == nullptr);
                const uint64_t timestamp = resolveTimestamp(block.timecode);

                for (auto &[data, duration]: block.frames)
                {
                    AudioFrame af;
                    af.trackNumber = block.trackNumber;
                    af.timestamp   = timestamp;
                    af.keyframe    = isKeyframe;
                    af.data.assign(data.begin(), data.end());
                    allFrames_.push_back(std::move(af));
                }
            }
        }
    }

    bool MatroskaAudioLoader::ReadNextFrame(AudioFrame &frame)
    {
        if (frameCursor_ >= allFrames_.size())
            return false;

        frame = allFrames_[frameCursor_++];
        return true;
    }

    void MatroskaAudioLoader::SeekToTimestamp(uint64_t timestamp)
    {
        // Linear scan from the start: frames across multiple interleaved tracks aren't guaranteed strictly monotonic as
        // a single sequence, so a binary search isn't safe here in general. Cheap enough for typical audio-track frame
        // counts.
        for (frameCursor_ = 0; frameCursor_ < allFrames_.size(); ++frameCursor_)
            if (allFrames_[frameCursor_].timestamp >= timestamp)
                return;
    }

    void MatroskaAudioLoader::PrintInfo() const
    {
        std::cout << "\n=== Summary ===" << "\n";
        std::cout << "Total Audio Tracks: " << audioTracks.size() << "\n";

        if (!segmentInfo.title.empty())
            std::cout << "Title: " << segmentInfo.title << "\n";

        if (segmentInfo.duration > 0)
            std::cout << "Duration: " << std::fixed << std::setprecision(2) << segmentInfo.duration << " seconds"
                      << "\n";

        for (const auto &t: audioTracks)
        {
            std::cout << "  Track " << t.trackNumber << ": " << t.codecID << " " << t.samplingFrequency << "Hz "
                      << t.channels << "ch" << (t.isDefault ? " [default]" : "") << "\n";
        }
    }
} // namespace SF::Engine
