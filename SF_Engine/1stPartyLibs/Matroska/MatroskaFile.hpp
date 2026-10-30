#pragma once

#include <chrono>
#include <memory>
#include <random>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include "Block.hpp"
#include "Cues.hpp"
#include "MatroskaIds.hpp"
#include "MatroskaSchema.hpp"
#include "Streaming.hpp"
#include "Timestamp.hpp"
#include "Track.hpp"

using namespace SF::EBML;
namespace SF::Matroska
{
    using namespace std;
    class MatroskaFile
    {
        Schema schema_;
        Element info_;
        Element tracksElement_;
        std::vector<Element> clusters_;

        Timestamp timestamp_;
        std::unordered_map<uint64_t, Track> tracksMap_;
        uint64_t nextTrackNumber_ = 1;

        // Track positions for cues
        std::vector<CuePoint> cuePoints_;
        uint64_t clusterTimecode_    = 0;
        size_t clusterStartPosition_ = 0;
        uint64_t blockNumber_        = 0;

        // Segment-relative byte offsets of Info/Tracks, captured as they're
        // written, so a populated SeekHead can be emitted once Cues (whose
        // own position isn't known until finalize()) is written too.
        size_t segmentDataStart_ = 0;
        size_t infoPos_          = 0;
        size_t tracksPos_        = 0;

        // File duration tracking (end of the last frame, incl. its duration)
        std::chrono::nanoseconds duration_{0};
        size_t durationPayloadPos_ = 0; // absolute offset of Info/Duration's 8-byte payload

        // Cluster management
        bool clusterOpen_         = false;
        uint64_t maxClusterTicks_ = 0; // 0 = derive from timecode scale (5 s, capped to the int16 block range)
        std::unordered_set<uint64_t> cuedTracksInCluster_;

        // Random generator for UIDs
        std::mt19937_64 rng_{std::random_device{}()};

        // Streaming writer
        std::unique_ptr<StreamingWriter> writer_;
        bool finalized_ = false;

    public:
        MatroskaFile() :
            schema_(create_matroska_schema()), info_(Element::make_master(ids::Info)),
            tracksElement_(Element::make_master(ids::Tracks))
        {
        }

        ~MatroskaFile()
        {
            if (writer_ && !finalized_)
            {
                try
                {
                    finalize();
                } catch (...)
                {
                }
            }
        }

        void initialize_writer(std::ostream &out)
        {
            writer_ = std::make_unique<StreamingWriter>(out);
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
            if (writer_)
                throw std::runtime_error("Tracks must be added before initialize_writer()");
            if (track.number == 0)
                throw std::runtime_error("Track number cannot be 0");

            auto entry = create_track_entry(track);
            tracksElement_.add(std::move(entry));
            tracksMap_[track.number] = track;

            if (track.number >= nextTrackNumber_)
                nextTrackNumber_ = track.number + 1;
        }

        Track add_video_track(int width, int height, const std::string &codecID,
                              const std::vector<byte> &codecPrivate = {})
        {
            Track track;
            track.number  = nextTrackNumber_++;
            track.uid     = generate_uid();
            track.type    = TrackType::Video;
            track.codecID = codecID;

            VideoTrack video;
            video.pixelWidth   = width;
            video.pixelHeight  = height;
            video.codecPrivate = codecPrivate;
            video.codecID      = codecID;
            track.specific     = std::move(video);

            add_track(track);
            return track;
        }

        Track add_audio_track(int sampleRate, int channels, const std::string &codecID,
                              const std::vector<EBML::byte> &codecPrivate = {})
        {
            Track track;
            track.number  = nextTrackNumber_++;
            track.uid     = generate_uid();
            track.type    = TrackType::Audio;
            track.codecID = codecID;

            AudioTrack audio;
            audio.samplingFrequency = sampleRate;
            audio.channels          = channels;
            audio.codecPrivate      = codecPrivate;
            audio.codecID           = codecID;
            track.specific          = std::move(audio);

            add_track(track);
            return track;
        }

        struct AudioTrackOptions
        {
            std::vector<EBML::byte> codecPrivate;
            std::chrono::nanoseconds codecDelay{0};      // Opus: pre-skip in ns
            std::chrono::nanoseconds seekPreRoll{0};     // Opus: 80 ms
            std::chrono::nanoseconds defaultDuration{0}; // e.g. 20 ms for 20 ms Opus packets
            uint64_t outputSamplingFrequency = 0;
            uint64_t bitDepth                = 0;
            std::string language             = "eng";
            std::string name;
        };

        Track add_audio_track(double sampleRate, int channels, const std::string &codecID, const AudioTrackOptions &opt)
        {
            Track track;
            track.number          = nextTrackNumber_++;
            track.uid             = generate_uid();
            track.type            = TrackType::Audio;
            track.codecID         = codecID;
            track.language        = opt.language;
            track.name            = opt.name;
            track.codecDelay      = opt.codecDelay;
            track.seekPreRoll     = opt.seekPreRoll;
            track.defaultDuration = opt.defaultDuration;
            track.lacing          = false;

            AudioTrack audio;
            audio.samplingFrequency       = sampleRate;
            audio.outputSamplingFrequency = opt.outputSamplingFrequency;
            audio.channels                = static_cast<uint64_t>(channels);
            audio.bitDepth                = opt.bitDepth;
            audio.codecPrivate            = opt.codecPrivate;
            audio.codecID                 = codecID;
            track.specific                = std::move(audio);

            add_track(track);
            return track;
        }

        void start_cluster(int64_t timecode)
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");

            clusterTimecode_      = static_cast<uint64_t>(timecode);
            clusterStartPosition_ = writer_->tell();

            timestamp_.set_cluster_start(std::chrono::nanoseconds(timecode * timestamp_.timecode_scale()));

            writer_->begin_unknown(ids::Cluster, 8, false);

            auto clusterTimecode = Element::make_uint(ids::Timecode, static_cast<uint64_t>(timecode));
            writer_->write_element(clusterTimecode);

            blockNumber_ = 0;
            clusterOpen_ = true;
            cuedTracksInCluster_.clear();
        }

        void end_cluster()
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");

            if (clusterOpen_)
            {
                writer_->end_unknown();
                clusterOpen_ = false;
            }
        }

        // Optional: override the automatic cluster length (in timecode ticks). Must stay < 32767.
        void set_max_cluster_ticks(uint64_t ticks) { maxClusterTicks_ = ticks; }

        /**
         * @brief Writes one frame/packet. Clusters are opened and rotated automatically.
         * @param trackNumber track number
         * @param timestamp      Absolute time in timecode ticks (default 1 tick = 1 ms).
         * @param data
         * @param keyframe
         * @param durationTicks  Optional. Recorded as BlockDuration and used for the file Duration.
         * @param discardPaddingNs Optional (Opus): ns to trim from the END of this packet's decoded output.
         *        Either of the last two makes the frame a BlockGroup instead of a SimpleBlock.
         */
        void write_frame(uint64_t trackNumber, int64_t timestamp, std::span<const byte> data, bool keyframe = true,
                         uint64_t durationTicks = 0, int64_t discardPaddingNs = 0)
        {
            if (!writer_)
                throw std::runtime_error("Writer not initialized");
            if (finalized_)
                throw std::runtime_error("File already finalized");
            auto trackIt = tracksMap_.find(trackNumber);
            if (trackNumber == 0 || trackIt == tracksMap_.end())
                throw std::runtime_error("Invalid track number");

            const uint64_t scale = timestamp_.timecode_scale();
            if (maxClusterTicks_ == 0)
                maxClusterTicks_ = std::min<uint64_t>(30000, std::max<uint64_t>(1, 5'000'000'000ull / scale));

            if (!clusterOpen_ || timestamp < static_cast<int64_t>(clusterTimecode_) ||
                timestamp - static_cast<int64_t>(clusterTimecode_) >= static_cast<int64_t>(maxClusterTicks_))
            {
                end_cluster();
                start_cluster(timestamp);
            }

            Block block;
            block.trackNumber = trackNumber;
            block.timecode    = timestamp_.block_timecode(std::chrono::nanoseconds(timestamp * scale));
            block.lacing      = LacingType::None;
            Frame frame;
            frame.data.assign(data.begin(), data.end());
            block.frames.push_back(std::move(frame));

            if (durationTicks == 0 && discardPaddingNs == 0)
            {
                SimpleBlock sb;
                sb.trackNumber = block.trackNumber;
                sb.timecode    = block.timecode;
                sb.keyframe    = keyframe;
                sb.frames      = std::move(block.frames);
                writer_->write_simple_block(sb);
            } else
            {
                auto group = Element::make_master(ids::BlockGroup);
                group.add(Element::make_binary(ids::Block, encode_block(block)));
                if (durationTicks)
                    group.add(Element::make_uint(ids::BlockDuration, durationTicks));
                if (!keyframe)
                    group.add(Element::make_int(ids::ReferenceBlock, -1)); // any reference => not a keyframe
                if (discardPaddingNs)
                    group.add(Element::make_int(ids::DiscardPadding, discardPaddingNs));
                writer_->write_element(group);
            }
            blockNumber_++;

            auto end = timestamp_.from_block_timecode(block.timecode) + std::chrono::nanoseconds(durationTicks * scale);
            if (end > duration_)
                duration_ = end;

            // Cues: every keyframe for video, once per cluster for everything else (audio frames are all
            // keyframes, a CuePoint per packet would bloat the file).
            const bool isVideo = trackIt->second.type == TrackType::Video;
            if (keyframe && (isVideo || cuedTracksInCluster_.insert(trackNumber).second))
            {
                CuePoint point;
                point.timecode = static_cast<uint64_t>(timestamp);
                CuePoint::TrackPosition pos;
                pos.trackNumber     = trackNumber;
                pos.clusterPosition = clusterStartPosition_ - segmentDataStart_; // Segment-relative, per spec
                pos.blockNumber     = blockNumber_;                              // 1-based within the cluster
                point.positions.push_back(pos);
                cuePoints_.push_back(point);
            }
        }

        void finalize(std::ostream *out = nullptr)
        {
            if (finalized_)
                return;
            if (!writer_)
                throw std::runtime_error("Writer not initialized");

            // Close the open cluster before Cues/SeekHead (they are siblings of Cluster, not children).
            end_cluster();

            // Patch Info/Duration (a float64 placeholder written up front). Per spec it is in timecode
            // ticks, not seconds. Silently skipped on non-seekable streams.
            if (duration_.count() > 0 && durationPayloadPos_ != 0)
            {
                const auto ticks =
                        static_cast<double>(duration_.count()) / static_cast<double>(timestamp_.timecode_scale());
                const auto bytes = encode_float(ticks);
                writer_->patch_bytes(durationPayloadPos_, bytes);
            }

            // Generate and write Cues, tracking its Segment-relative offset
            // so the trailing SeekHead below can point at it.
            size_t cuesPos = 0;
            bool haveCues  = false;
            if (!cuePoints_.empty())
            {
                cuesPos          = writer_->tell() - segmentDataStart_;
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

        const std::unordered_map<uint64_t, Track> &get_tracks() const { return tracksMap_; }

        const Element &get_tracks_element() const { return tracksElement_; }

        const std::vector<CuePoint> &get_cue_points() const { return cuePoints_; }

        void set_timecode_scale(uint64_t scale) { timestamp_.set_timecode_scale(scale); }

        uint64_t get_timecode_scale() const { return timestamp_.timecode_scale(); }

    private:
        uint64_t generate_uid() { return rng_(); }

        void write_ebml_header() const
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
            info_.add(Element::make_float(ids::Duration, 0.0)); // 8-byte placeholder, patched in finalize()
            info_.add(Element::make_utf8(ids::MuxingApp, "SF::EBML/Matroska"));
            info_.add(Element::make_utf8(ids::WritingApp, "SF::EBML/Matroska"));
            info_.add(Element::make_date(ids::DateUTC, std::chrono::system_clock::now()));

            // Generate SegmentUID
            std::vector<byte> uid(16);
            for (auto &b: uid)
                b = static_cast<byte>(rng_() & 0xFF);
            info_.add(Element::make_binary(ids::SegmentUID, std::move(uid)));

            SerializationContext ctx;
            ctx.currentPosition = writer_->tell();
            (void) serialize(info_, &ctx); // only to learn where Duration lands
            durationPayloadPos_ = ctx.elementPositions.at(ids::Duration.value()) + ids::Duration.encode().size() +
                                  encode_size(8).size();

            writer_->write_element(info_);
        }

        void write_tracks() const { writer_->write_element(tracksElement_); }
    };
} // namespace SF::Matroska
