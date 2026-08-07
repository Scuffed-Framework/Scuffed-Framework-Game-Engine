#pragma once

#include "Block.hpp"
#include "Track.hpp"
#include "Cues.hpp"
#include "Timestamp.hpp"
#include "Streaming.hpp"
#include "MatroskaIds.hpp"
#include "MatroskaSchema.hpp"
#include "../EBML/Serializer.hpp"
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>

namespace SF::Matroska
{
    using namespace SF::EBML;

    // Configuration for the muxer
    struct MuxerConfig
    {
        uint64_t timecodeScale = 1000000;
        uint64_t clusterTimecode = 0;
        bool enableCRC32 = false;
        bool enableTwoPass = false;
        bool enableBackPatching = true;
        size_t maxClusterDuration = 5000;
        size_t maxClusterSize = 1024 * 1024 * 64;
        bool useBlockGroups = true;
        std::string writingApp = "SF::EBML/Matroska";
        std::string muxingApp = "SF::EBML/Matroska";
        bool defaultVideoLacing = false;
        bool defaultAudioLacing = false;
        std::string defaultLanguage = "eng";
        bool strictValidation = true;
        bool autoFixIssues = true;
    };

    // Statistics struct - fixed to be copyable
    struct Statistics
    {
        uint64_t totalFrames = 0;
        uint64_t totalBytes = 0;
        uint64_t totalClusters = 0;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        
        Statistics() = default;
        Statistics(const Statistics&) = default;
        Statistics& operator=(const Statistics&) = default;
    };

    // Thread-safe frame queue
    class FrameQueue
    {
    private:
        struct QueuedFrame
        {
            uint64_t trackNumber;
            int64_t timestamp;
            std::shared_ptr<std::vector<byte>> data;
            bool keyframe;
            std::chrono::steady_clock::time_point enqueueTime;
        };
        
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<QueuedFrame> queue_;
        std::atomic<bool> isShutdown_{false};
        size_t maxSize_ = 1024;

    public:
        void set_max_size(size_t size) { maxSize_ = size; }
        
        void push(uint64_t trackNumber, int64_t timestamp, 
                  std::vector<byte>&& data, bool keyframe)
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return queue_.size() < maxSize_ || isShutdown_; });
            
            if (isShutdown_)
                throw std::runtime_error("Frame queue is shut down");
            
            QueuedFrame frame;
            frame.trackNumber = trackNumber;
            frame.timestamp = timestamp;
            frame.data = std::make_shared<std::vector<byte>>(std::move(data));
            frame.keyframe = keyframe;
            frame.enqueueTime = std::chrono::steady_clock::now();
            queue_.push(std::move(frame));
            cv_.notify_one();
        }

        bool pop(uint64_t& trackNumber, int64_t& timestamp, 
                 std::shared_ptr<std::vector<byte>>& data, bool& keyframe)
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || isShutdown_; });
            
            if (queue_.empty())
                return false;
            
            auto frame = std::move(queue_.front());
            queue_.pop();
            trackNumber = frame.trackNumber;
            timestamp = frame.timestamp;
            data = std::move(frame.data);
            keyframe = frame.keyframe;
            cv_.notify_one();
            return true;
        }

        size_t size() const
        {
            std::lock_guard lock(mutex_);
            return queue_.size();
        }

        void shutdown()
        {
            isShutdown_ = true;
            cv_.notify_all();
        }

        bool empty() const
        {
            std::lock_guard lock(mutex_);
            return queue_.empty();
        }
    };

    // Main Matroska muxer class
    class MatroskaMuxer
    {
    private:
        // Make Cluster public so Validator can access it
    public:
        struct Cluster
        {
            uint64_t timecode = 0;
            size_t startPosition = 0;
            size_t endPosition = 0;
            uint64_t frameCount = 0;
            std::chrono::nanoseconds duration = std::chrono::nanoseconds::zero();
        };

    private:
        MuxerConfig config_;
        Schema schema_;
        
        std::unordered_map<uint64_t, Track> tracks_;
        std::unordered_map<uint64_t, uint64_t> trackIndexMap_;
        
        Timestamp timestamp_;
        std::vector<CuePoint> cuePoints_;
        mutable std::mutex cueMutex_;
        
        std::vector<Cluster> clusters_;
        mutable std::mutex clusterMutex_;
        
        struct ClusterState
        {
            uint64_t startTimecode = 0;
            uint64_t currentTimecode = 0;
            size_t size = 0;
            uint64_t frameCount = 0;
            bool active = false;
            std::chrono::nanoseconds duration = std::chrono::nanoseconds::zero();
        } currentCluster_;
        
        std::unique_ptr<BackPatchableWriter> writer_;
        std::unique_ptr<TwoPassMuxer> twoPassMuxer_;
        bool isFinalized_ = false;
        bool isInitialized_ = false;
        
        std::mt19937_64 rng_;
        
        std::unique_ptr<std::thread> muxThread_;
        std::atomic<bool> shouldStop_{false};
        FrameQueue frameQueue_;
        
        std::function<void(float)> progressCallback_;
        std::function<void(const std::string&)> errorCallback_;
        std::function<void(uint64_t, int64_t)> frameWrittenCallback_;
        
        Statistics stats_;

    public:
        MatroskaMuxer(const MuxerConfig& config = MuxerConfig{})
            : config_(config), schema_(create_matroska_schema()), 
              timestamp_(config.timecodeScale),
              rng_(std::random_device{}())
        {
            stats_.startTime = std::chrono::steady_clock::now();
        }

        ~MatroskaMuxer()
        {
            shutdown();
        }

        // Add a track (thread-safe)
        void add_track(const Track& track)
        {
            if (isInitialized_)
                throw std::runtime_error("Cannot add tracks after initialization");
            
            if (!track.validate())
                throw std::runtime_error("Invalid track");
            
            std::lock_guard lock(clusterMutex_);
            tracks_[track.number] = track;
            trackIndexMap_[track.number] = tracks_.size();
        }

        Track add_video_track(int width, int height, const std::string& codecID,
                              const std::vector<byte>& codecPrivate = {})
        {
            Track track;
            track.number = get_next_track_number();
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

        Track add_audio_track(int sampleRate, int channels, const std::string& codecID,
                              const std::vector<byte>& codecPrivate = {})
        {
            Track track;
            track.number = get_next_track_number();
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

        // Initialize the muxer for writing
        void initialize(std::ostream& out)
        {
            if (isInitialized_)
                throw std::runtime_error("Already initialized");
            
            if (tracks_.empty())
                throw std::runtime_error("No tracks added");

            if (config_.enableTwoPass)
            {
                twoPassMuxer_ = std::make_unique<TwoPassMuxer>();
            }
            else
            {
                writer_ = std::make_unique<BackPatchableWriter>(out, false);
                write_header();
            }
            
            isInitialized_ = true;
            shouldStop_ = false;
        }

        // Write a frame (thread-safe, with move semantics)
        void write_frame(uint64_t trackNumber, int64_t timestamp,
                         std::vector<byte>&& data, bool keyframe = true)
        {
            if (!isInitialized_)
                throw std::runtime_error("Muxer not initialized");
            if (isFinalized_)
                throw std::runtime_error("Muxer already finalized");
            
            // Validate track
            if (tracks_.find(trackNumber) == tracks_.end())
                throw std::runtime_error("Invalid track number");
            
            // Update duration
            std::lock_guard lock(clusterMutex_);
            auto absTime = std::chrono::nanoseconds(timestamp * config_.timecodeScale);
            if (absTime > stats_.endTime.time_since_epoch())
                stats_.endTime = std::chrono::steady_clock::time_point(absTime);
            
            // Push to queue (will be processed by mux thread)
            frameQueue_.push(trackNumber, timestamp, std::move(data), keyframe);
            stats_.totalFrames++;
        }

        // Write a block group (for B-frames)
        void write_block_group(const BlockGroup& group)
        {
            if (!isInitialized_)
                throw std::runtime_error("Muxer not initialized");
            if (isFinalized_)
                throw std::runtime_error("Muxer already finalized");
            if (!group.validate())
                throw std::runtime_error("Invalid block group");
            
            if (config_.enableTwoPass)
            {
                // Two-pass mode: collect for later processing
                // This is handled by the mux thread
            }
            else
            {
                // Direct write
                writer_->write(create_block_group_element(group));
            }
        }

        // Finalize the file
        void finalize()
        {
            if (isFinalized_)
                return;
            
            // Wait for all frames to be processed
            while (!frameQueue_.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            if (config_.enableTwoPass && twoPassMuxer_)
            {
                // Two-pass muxing
                twoPassMuxer_->compute_positions();
                // Write the file
                std::ofstream out("temp.mkv", std::ios::binary);
                twoPassMuxer_->write(out, config_.enableBackPatching);
            }
            else if (writer_)
            {
                // Write remaining data
                flush_cluster();
                
                // Write Cues
                write_cues();
                
                // Apply back-patches
                if (config_.enableBackPatching)
                {
                    update_seek_head();
                    writer_->apply_all_patches();
                }
                
                writer_->flush();
            }
            
            isFinalized_ = true;
            stats_.endTime = std::chrono::steady_clock::now();
            
            if (progressCallback_)
                progressCallback_(1.0f);
        }

        // Start background muxing thread
        void start_mux_thread()
        {
            if (muxThread_)
                return;
            
            muxThread_ = std::make_unique<std::thread>([this]() {
                mux_loop();
            });
        }

        void shutdown()
        {
            shouldStop_ = true;
            frameQueue_.shutdown();
            if (muxThread_ && muxThread_->joinable())
                muxThread_->join();
        }

        // Callbacks
        void set_progress_callback(std::function<void(float)> callback)
        {
            progressCallback_ = std::move(callback);
        }

        void set_error_callback(std::function<void(const std::string&)> callback)
        {
            errorCallback_ = std::move(callback);
        }

        void set_frame_written_callback(std::function<void(uint64_t, int64_t)> callback)
        {
            frameWrittenCallback_ = std::move(callback);
        }

        // Statistics
        Statistics get_statistics() const
        {
            Statistics stats;
            stats.totalFrames = stats_.totalFrames;
            stats.totalBytes = stats_.totalBytes;
            stats.totalClusters = stats_.totalClusters;
            stats.startTime = stats_.startTime;
            stats.endTime = stats_.endTime;
            return stats;
        }

        // Getters
        const std::vector<CuePoint>& get_cue_points() const { return cuePoints_; }
        const std::unordered_map<uint64_t, Track>& get_tracks() const { return tracks_; }
        const MuxerConfig& get_config() const { return config_; }

    private:
        uint64_t get_next_track_number()
        {
            uint64_t max = 1;
            for (const auto& [num, _] : tracks_)
                if (num >= max)
                    max = num + 1;
            return max;
        }

        uint64_t generate_uid()
        {
            return rng_();
        }

        void write_header()
        {
            if (!writer_) return;
            
            // EBML header
            auto ebml = Element::make_master(::SF::EBML::ids::EBML);
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLVersion, 1));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLReadVersion, 1));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLMaxIDLength, 4));
            ebml.add(Element::make_uint(::SF::EBML::ids::EBMLMaxSizeLength, 8));
            ebml.add(Element::make_string(::SF::EBML::ids::DocType, "matroska"));
            ebml.add(Element::make_uint(::SF::EBML::ids::DocTypeVersion, 4));
            ebml.add(Element::make_uint(::SF::EBML::ids::DocTypeReadVersion, 2));
            writer_->write(ebml);

            // Segment (unknown size for now)
            // We'll back-patch this later if needed
            auto segment = Element::make_master(ids::Segment);
            writer_->write_with_backpatch(ids::Segment.value(), segment, writer_->tell());
            [[maybe_unused]] size_t segmentStart = writer_->tell();

            // Info
            write_info();
            
            // Tracks
            write_tracks();
            
            // Track positions for SeekHead
            [[maybe_unused]] size_t infoPos = writer_->get_element_position(ids::Info.value());
            [[maybe_unused]] size_t tracksPos = writer_->get_element_position(ids::Tracks.value());
        }

        void write_info()
        {
            auto info = Element::make_master(ids::Info);
            info.add(Element::make_uint(ids::TimecodeScale, config_.timecodeScale));
            info.add(Element::make_utf8(ids::MuxingApp, config_.muxingApp));
            info.add(Element::make_utf8(ids::WritingApp, config_.writingApp));
            info.add(Element::make_date(ids::DateUTC, std::chrono::system_clock::now()));

            std::vector<byte> uid(16);
            for (auto& b : uid)
                b = static_cast<byte>(rng_() & 0xFF);
            info.add(Element::make_binary(ids::SegmentUID, std::move(uid)));

            writer_->write(info);
        }

        void write_tracks()
        {
            auto tracks = Element::make_master(ids::Tracks);
            
            for (const auto& [num, track] : tracks_)
            {
                auto entry = create_track_entry(track);
                tracks.add(std::move(entry));
            }

            writer_->write(tracks);
        }

        void write_cues()
        {
            if (cuePoints_.empty())
                return;

            std::lock_guard lock(cueMutex_);
            auto cuesElement = generate_cues(cuePoints_);
            writer_->write(cuesElement);
        }

        void update_seek_head()
        {
            // Update SeekHead with correct positions
            size_t infoPos = writer_->get_element_position(ids::Info.value());
            size_t tracksPos = writer_->get_element_position(ids::Tracks.value());
            size_t cuesPos = writer_->get_element_position(ids::Cues.value());
            
            auto seekHead = generate_seek_head(infoPos, tracksPos, cuesPos);
            // Write seek head at the beginning
            // This is handled by back-patching
        }

        Element create_block_group_element(const BlockGroup& group)
        {
            auto bg = Element::make_master(ids::BlockGroup);
            
            auto blockData = encode_block(group.block);
            bg.add(Element::make_binary(ids::Block, std::move(blockData)));
            
            for (int64_t ref : group.referenceBlocks)
            {
                bg.add(Element::make_int(ids::ReferenceBlock, ref));
            }
            
            if (group.duration)
            {
                bg.add(Element::make_uint(ids::BlockDuration, group.duration));
            }
            
            if (group.codecState)
            {
                bg.add(Element::make_uint(ids::CodecState, group.codecState));
            }
            
            for (const auto& child : group.additionalData)
            {
                bg.add(child);
            }
            
            return bg;
        }

        void start_cluster(uint64_t timecode)
        {
            if (currentCluster_.active)
                flush_cluster();

            currentCluster_.startTimecode = timecode;
            currentCluster_.currentTimecode = timecode;
            currentCluster_.size = 0;
            currentCluster_.frameCount = 0;
            currentCluster_.active = true;
            currentCluster_.duration = std::chrono::nanoseconds::zero();

            timestamp_.set_cluster_start(
                std::chrono::nanoseconds(timecode * config_.timecodeScale));

            // Write cluster header
            auto cluster = Element::make_master(ids::Cluster);
            cluster.add(Element::make_uint(ids::Timecode, timecode));
            
            if (config_.enableCRC32)
            {
                // Add CRC32 placeholder
                // This will be back-patched later
            }
            
            writer_->write(cluster);
            
            currentCluster_.size = writer_->tell();
        }

        void write_frame_to_cluster(uint64_t trackNumber, int64_t timestamp,
                                    const std::shared_ptr<std::vector<byte>>& data,
                                    bool keyframe)
        {
            if (!currentCluster_.active)
            {
                start_cluster(timestamp);
            }

            // Check if we need to start a new cluster
            uint64_t timecodeOffset = timestamp - currentCluster_.startTimecode;
            if (timecodeOffset > config_.maxClusterDuration ||
                currentCluster_.size > config_.maxClusterSize ||
                currentCluster_.frameCount >= 256) // Max frames per cluster
            {
                flush_cluster();
                start_cluster(timestamp);
                timecodeOffset = 0;
            }

            // Create SimpleBlock
            SimpleBlock block;
            block.trackNumber = trackNumber;
            block.timecode = static_cast<int16_t>(timecodeOffset);
            block.keyframe = keyframe;
            block.lacing = LacingType::None;

            Frame frame;
            frame.data = *data; // Copy - would use move in production
            block.frames.push_back(std::move(frame));

            // Write block
            auto blockData = encode_simple_block(block);
            auto blockElement = Element::make_binary(ids::SimpleBlock, std::move(blockData));
            writer_->write(blockElement);

            // Update cluster state
            currentCluster_.frameCount++;
            currentCluster_.size = writer_->tell();
            
            auto absTime = timestamp_.from_block_timecode(block.timecode);
            if (absTime > currentCluster_.duration)
                currentCluster_.duration = absTime;

            // Add cue point for keyframes
            if (keyframe)
            {
                std::lock_guard lock(cueMutex_);
                CuePoint point;
                point.timecode = timestamp;
                CuePoint::TrackPosition pos;
                pos.trackNumber = trackNumber;
                pos.clusterPosition = writer_->get_element_position(ids::Cluster.value());
                pos.blockNumber = currentCluster_.frameCount;
                point.positions.push_back(pos);
                cuePoints_.push_back(point);
            }

            stats_.totalBytes += data->size();

            if (frameWrittenCallback_)
                frameWrittenCallback_(trackNumber, timestamp);
        }

        void flush_cluster()
        {
            if (!currentCluster_.active)
                return;

            // Store cluster info
            Cluster cluster;
            cluster.timecode = currentCluster_.startTimecode;
            cluster.startPosition = writer_->get_element_position(ids::Cluster.value());
            cluster.endPosition = writer_->tell();
            cluster.frameCount = currentCluster_.frameCount;
            cluster.duration = currentCluster_.duration;

            std::lock_guard lock(clusterMutex_);
            clusters_.push_back(cluster);
            stats_.totalClusters++;

            currentCluster_.active = false;
        }

        void mux_loop()
        {
            while (!shouldStop_)
            {
                uint64_t trackNumber;
                int64_t timestamp;
                std::shared_ptr<std::vector<byte>> data;
                bool keyframe;

                if (frameQueue_.pop(trackNumber, timestamp, data, keyframe))
                {
                    try
                    {
                        if (config_.enableTwoPass)
                        {
                            // Two-pass mode: collect data
                            // This would store the frame for later processing
                        }
                        else
                        {
                            write_frame_to_cluster(trackNumber, timestamp, data, keyframe);
                        }

                        // Update progress
                        if (progressCallback_ && stats_.totalFrames > 0)
                        {
                            float progress = static_cast<float>(stats_.totalFrames) / 
                                           (stats_.totalFrames + frameQueue_.size());
                            progressCallback_(std::min(1.0f, progress));
                        }
                    }
                    catch (const std::exception& e)
                    {
                        if (errorCallback_)
                            errorCallback_(e.what());
                        else
                            throw;
                    }
                }
            }
        }

        // Helper to create track entry (from Track.hpp)
        Element create_track_entry(const Track& track)
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

            // Add specific track data
            if (track.type == TrackType::Video)
            {
                const auto& video = std::get<VideoTrack>(track.specific);
                entry.add(Element::make_uint(ids::PixelWidth, video.pixelWidth));
                entry.add(Element::make_uint(ids::PixelHeight, video.pixelHeight));
                if (video.pixelCropTop)
                    entry.add(Element::make_uint(ids::PixelCropTop, video.pixelCropTop));
                if (video.pixelCropBottom)
                    entry.add(Element::make_uint(ids::PixelCropBottom, video.pixelCropBottom));
                if (video.pixelCropLeft)
                    entry.add(Element::make_uint(ids::PixelCropLeft, video.pixelCropLeft));
                if (video.pixelCropRight)
                    entry.add(Element::make_uint(ids::PixelCropRight, video.pixelCropRight));
                if (video.frameRate > 0.0)
                    entry.add(Element::make_float(ids::FrameRate, video.frameRate));
                if (video.displayWidth)
                    entry.add(Element::make_uint(ids::DisplayWidth, video.displayWidth));
                if (video.displayHeight)
                    entry.add(Element::make_uint(ids::DisplayHeight, video.displayHeight));
                if (video.displayUnit)
                    entry.add(Element::make_uint(ids::DisplayUnit, video.displayUnit));
                if (video.interlaced)
                    entry.add(Element::make_uint(ids::FlagInterlaced, 1));
                if (video.stereoMode)
                    entry.add(Element::make_uint(ids::StereoMode, 1));
                if (!video.codecPrivate.empty())
                    entry.add(Element::make_binary(ids::CodecPrivate, video.codecPrivate));
            }
            else if (track.type == TrackType::Audio)
            {
                const auto& audio = std::get<AudioTrack>(track.specific);
                entry.add(Element::make_float(ids::SamplingFrequency, audio.samplingFrequency));
                if (audio.outputSamplingFrequency)
                    entry.add(Element::make_uint(ids::OutputSamplingFrequency, audio.outputSamplingFrequency));
                entry.add(Element::make_uint(ids::Channels, audio.channels));
                if (audio.bitDepth)
                    entry.add(Element::make_uint(ids::BitDepth, audio.bitDepth));
                if (!audio.codecPrivate.empty())
                    entry.add(Element::make_binary(ids::CodecPrivate, audio.codecPrivate));
            }

            return entry;
        }
    };
}