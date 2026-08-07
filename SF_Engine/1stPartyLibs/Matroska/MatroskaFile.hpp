#pragma once

#include "Block.hpp"
#include "Track.hpp"
#include "Cues.hpp"
#include "Timestamp.hpp"
#include "Streaming.hpp"
#include "MatroskaIds.hpp"
#include "MatroskaSchema.hpp"
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>

using namespace SF::EBML;
namespace SF::Matroska
{
    class MatroskaFile
    {
    private:
        Schema schema_;
        Element info_;
        Element tracksElement_;
        std::vector<Element> clusters_;

        Timestamp timestamp_;
        std::unordered_map<uint64_t, Track> tracksMap_;
        uint64_t nextTrackNumber_ = 1;
        
        // Track positions for cues
        std::vector<CuePoint> cuePoints_;
        uint64_t clusterTimecode_ = 0;
        size_t clusterStartPosition_ = 0;
        uint64_t blockNumber_ = 0;

        // Segment-relative byte offsets of Info/Tracks, captured as they're
        // written, so a populated SeekHead can be emitted once Cues (whose
        // own position isn't known until finalize()) is written too.
        size_t segmentDataStart_ = 0;
        size_t infoPos_ = 0;
        size_t tracksPos_ = 0;
        
        // File duration tracking
        std::chrono::nanoseconds duration_{0};

        // Random generator for UIDs
        std::mt19937_64 rng_{std::random_device{}()};

        // Streaming writer
        std::unique_ptr<StreamingWriter> writer_;
        bool finalized_ = false;

    public:
        MatroskaFile()
            : schema_(create_matroska_schema()),
              info_(Element::make_master(ids::Info)),
              tracksElement_(Element::make_master(ids::Tracks))
        {}

        ~MatroskaFile()
        {
            if (writer_ && !finalized_)
            {
                try { finalize(); }
                catch (...) {}
            }
        }

        void initialize_writer(std::ostream &out)
        {
            writer_ = std::make_unique<StreamingWriter>(out, schema_);
            write_ebml_header();
            writer_->begin_unknown(ids::Segment, 8);
            segmentDataStart_ = writer_->tell();

            infoPos_ = writer_->tell() - segmentDataStart_;
            write_info();

            tracksPos_ = writer_->tell() - segmentDataStart_;
            write_tracks();

            // No SeekHead here: Cues' position isn't known until finalize(),
            // and we can't back-patch without assuming a seekable stream. A
            // fully populated SeekHead is written at the end of the Segment
            // instead (see finalize()) - valid per spec, and works over any
            // ostream including non-seekable ones.
        }

        void add_track(const Track &track)
        {
            if (track.number == 0)
                throw std::runtime_error("Track number cannot be 0");
            
            auto entry = create_track_entry(track);
            tracksElement_.add(std::move(entry));
            tracksMap_[track.number] = track;
            
            if (track.number >= nextTrackNumber_)
                nextTrackNumber_ = track.number + 1;
        }

        Track add_video_track(int width, int height, const std::string &codecID, 
                              const std::vector<byte>& codecPrivate = {})
        {
            Track track;
            track.number = nextTrackNumber_++;
            track.uid = generate_uid();
            track.type = TrackType::Video;
            track.codecID = codecID;

            VideoTrack video;
            video.pixelWidth = width;
            video.pixelHeight = height;
            video.codecPrivate = codecPrivate;
            video.codecID = codecID;
            track.specific = std::move(video);

            add_track(track);
            return track;
        }

        Track add_audio_track(int sampleRate, int channels, const std::string &codecID,
                              const std::vector<byte>& codecPrivate = {})
        {
            Track track;
            track.number = nextTrackNumber_++;
            track.uid = generate_uid();
            track.type = TrackType::Audio;
            track.codecID = codecID;

            AudioTrack audio;
            audio.samplingFrequency = sampleRate;
            audio.channels = channels;
            audio.codecPrivate = codecPrivate;
            audio.codecID = codecID;
            track.specific = std::move(audio);

            add_track(track);
            return track;
        }

        void start_cluster(int64_t timecode)
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");

            clusterTimecode_ = static_cast<uint64_t>(timecode);
            clusterStartPosition_ = writer_->tell();
            
            timestamp_.set_cluster_start(
                std::chrono::nanoseconds(timecode * timestamp_.timecode_scale()));

            writer_->begin_unknown(ids::Cluster, 8, false);

            auto clusterTimecode = Element::make_uint(ids::Timecode,
                                                      static_cast<uint64_t>(timecode));
            writer_->write_element(clusterTimecode);
            
            blockNumber_ = 0;
        }

        void end_cluster()
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");
            
            writer_->end_unknown();
        }

        void write_frame(uint64_t trackNumber, int64_t timestamp,
                         std::span<const byte> data, bool keyframe = true)
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");
            if (trackNumber == 0 || tracksMap_.find(trackNumber) == tracksMap_.end())
                throw std::runtime_error("Invalid track number");

            SimpleBlock block;
            block.trackNumber = trackNumber;
            block.timecode = timestamp_.block_timecode(
                std::chrono::nanoseconds(timestamp * timestamp_.timecode_scale()));
            block.keyframe = keyframe;
            block.lacing = LacingType::None;

            Frame frame;
            frame.data = std::vector<byte>(data.begin(), data.end());
            block.frames.push_back(std::move(frame));

            writer_->write_simple_block(block);
            blockNumber_++;
            
            // Update duration
            auto absTime = timestamp_.from_block_timecode(block.timecode);
            if (absTime > duration_)
                duration_ = absTime;

            // Add cue point for keyframes
            if (keyframe)
            {
                CuePoint point;
                point.timecode = static_cast<uint64_t>(timestamp);
                CuePoint::TrackPosition pos;
                pos.trackNumber = trackNumber;
                pos.clusterPosition = clusterStartPosition_;
                pos.blockNumber = blockNumber_;
                point.positions.push_back(pos);
                cuePoints_.push_back(point);
            }
        }

        void finalize(std::ostream* out = nullptr)
        {
            if (finalized_) return;
            if (!writer_) 
                throw std::runtime_error("Writer not initialized");

            // Write duration to Info
            if (duration_.count() > 0)
            {
                auto durationElement = Element::make_float(ids::Duration,
                    static_cast<double>(duration_.count()) / 1e9);
                // We'd need to update the Info element, but it's already written
                // For now, we'll just note it
            }

            // Generate and write Cues, tracking its Segment-relative offset
            // so the trailing SeekHead below can point at it.
            size_t cuesPos = 0;
            bool haveCues = false;
            if (!cuePoints_.empty())
            {
                cuesPos = writer_->tell() - segmentDataStart_;
                auto cuesElement = generate_cues(cuePoints_);
                writer_->write_element(cuesElement);
                haveCues = true;
            }

            // Populated SeekHead, written last now that every position it
            // references (Info, Tracks, Cues) is actually known.
            auto seekHead = generate_seek_head(infoPos_, tracksPos_,
                haveCues ? std::optional<uint64_t>(cuesPos) : std::nullopt);
            writer_->write_element(seekHead);

            // Close Segment
            writer_->end_unknown();
            writer_->flush();
            finalized_ = true;
        }

        const std::unordered_map<uint64_t, Track>& get_tracks() const 
        { 
            return tracksMap_; 
        }

        const Element& get_tracks_element() const 
        { 
            return tracksElement_; 
        }

        const std::vector<CuePoint>& get_cue_points() const 
        { 
            return cuePoints_; 
        }

        void set_timecode_scale(uint64_t scale)
        {
            timestamp_.set_timecode_scale(scale);
        }

        uint64_t get_timecode_scale() const
        {
            return timestamp_.timecode_scale();
        }

    private:
        uint64_t generate_uid()
        {
            return rng_();
        }

        void write_ebml_header()
        {
            auto ebml = Element::make_master(::SF::EBML::ids::EBML);
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLVersion, 1));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLReadVersion, 1));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLMaxIDLength, 4));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLMaxSizeLength, 8));
            ebml.add(Element::make_string(::SF::EBML::ids::DocType, "matroska"));
            ebml.add(Element::make_uint(::SF::EBML::ids::DocTypeVersion, 4));
            ebml.add(Element::make_uint(::SF::EBML::ids::DocTypeReadVersion, 2));

            writer_->write_element(ebml);
        }

        void write_info()
        {
            info_.add(Element::make_uint(ids::TimecodeScale, timestamp_.timecode_scale()));
            info_.add(Element::make_utf8(ids::MuxingApp, "SF::EBML/Matroska"));
            info_.add(Element::make_utf8(ids::WritingApp, "SF::EBML/Matroska"));
            info_.add(Element::make_date(ids::DateUTC, std::chrono::system_clock::now()));

            // Generate SegmentUID
            std::vector<byte> uid(16);
            for (auto& b : uid)
                b = static_cast<byte>(rng_() & 0xFF);
            info_.add(Element::make_binary(ids::SegmentUID, std::move(uid)));

            writer_->write_element(info_);
        }

        void write_tracks()
        {
            writer_->write_element(tracksElement_);
        }
    };
}