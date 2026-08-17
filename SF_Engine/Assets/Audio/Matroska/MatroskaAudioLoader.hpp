#ifndef MATROSKA_AUDIO_LOADER_HPP
#define MATROSKA_AUDIO_LOADER_HPP

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// 1st-party EBML/Matroska library
#include <1stPartyLibs/EBML/Element.hpp>
#include <1stPartyLibs/EBML/Schema.hpp>
#include <1stPartyLibs/Matroska/Block.hpp>
#include <1stPartyLibs/Matroska/MatroskaIds.hpp>
#include <1stPartyLibs/Matroska/MatroskaSchema.hpp>
#include <1stPartyLibs/Matroska/Streaming.hpp>
#include <1stPartyLibs/Matroska/Track.hpp>

namespace SF::Engine
{
    struct AudioTrackInfo
    {
        uint64_t trackNumber = 0;
        std::string codecID;
        std::string codecName;
        std::string language;
        double samplingFrequency = 48000.0;
        uint64_t channels = 2;
        uint64_t bitDepth = 0;
        std::vector<uint8_t> codecPrivate;
        bool enabled = true;
        bool isDefault = false;
    };

    struct SegmentInfo
    {
        std::string title;
        std::string muxingApp;
        std::string writingApp;
        double duration = 0.0;
        uint64_t timecodeScale = 1'000'000;
    };

    struct AudioFrame
    {
        uint64_t trackNumber = 0;
        uint64_t timestamp = 0;  // in segmentInfo.timecodeScale units, i.e. raw Matroska ticks
        std::vector<uint8_t> data;
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
        explicit MatroskaAudioLoader(const std::string& filePath);
        ~MatroskaAudioLoader();

        bool Load();
        void PrintInfo();

        const SegmentInfo& GetSegmentInfo() const { return segmentInfo; }
        const std::vector<AudioTrackInfo>& GetAudioTracks() const { return audioTracks; }

        bool ReadNextFrame(AudioFrame& frame);
        void SeekToTimestamp(uint64_t timestamp);

    private:
        std::string filePath;
        std::ifstream fileStream;

        SF::EBML::Schema schema{SF::Matroska::create_matroska_schema()};

        SegmentInfo segmentInfo;
        std::vector<AudioTrackInfo> audioTracks;

        // All audio-track frames extracted from the file, in file (i.e. time)
        // order, across every audio track. frameCursor_ is the read position
        // for ReadNextFrame()/SeekToTimestamp().
        std::vector<AudioFrame> allFrames_;
        std::size_t frameCursor_ = 0;

        bool loaded_ = false;

        void ParseInfo(const SF::EBML::Element& info);
        void ParseTracks(const SF::EBML::Element& tracks);
        void ParseTrackEntry(const SF::EBML::Element& entry);
        void ParseClusters(const SF::EBML::Element& segment);
        void ExtractFramesFromCluster(const SF::EBML::Element& cluster);

        bool IsAudioTrack(uint64_t trackNumber) const;

        void Reset();
    };
}  // namespace SF::Engine

#endif  // MATROSKA_AUDIO_LOADER_HPP