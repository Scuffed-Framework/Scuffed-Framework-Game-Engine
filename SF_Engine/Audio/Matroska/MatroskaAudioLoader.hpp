#ifndef MATROSKA_AUDIO_LOADER_HPP
#define MATROSKA_AUDIO_LOADER_HPP

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// EBML includes
#include <Matroska/EBML/EbmlHead.h>
#include <Matroska/EBML/EbmlStream.h>
#include <Matroska/EBML/StdIOCallback.h>

// Matroska includes
#include <Matroska/Matroska/KaxBlock.h>
#include <Matroska/Matroska/KaxBlockData.h>
#include <Matroska/Matroska/KaxCluster.h>
#include <Matroska/Matroska/KaxCues.h>
#include <Matroska/Matroska/KaxSeekHead.h>
#include <Matroska/Matroska/KaxSegment.h>
#include <Matroska/Matroska/KaxTracks.h>

#ifndef uint64
using uint64 = uint64_t;
#endif

using namespace libmatroska;
using namespace libebml;

namespace SF::Engine
{
    struct AudioTrackInfo
    {
        uint64_t trackNumber;
        std::string codecID;
        std::string codecName;
        std::string language;
        double samplingFrequency;
        uint64_t channels;
        uint64_t bitDepth;
        std::vector<uint8_t> codecPrivate;
        bool enabled;
        bool isDefault;
    };

    struct SegmentInfo
    {
        std::string title;
        std::string muxingApp;
        std::string writingApp;
        double duration;
        uint64_t timecodeScale;
    };

    struct AudioFrame
    {
        uint64_t trackNumber;
        uint64_t timestamp;
        std::vector<uint8_t> data;
        bool keyframe;
    };

    class MatroskaAudioLoader
    {
    public:
        MatroskaAudioLoader(const std::string& filePath);
        ~MatroskaAudioLoader();

        bool Load();
        void PrintInfo();

        const SegmentInfo& GetSegmentInfo() const
        {
            return segmentInfo;
        }
        const std::vector<AudioTrackInfo>& GetAudioTracks() const
        {
            return audioTracks;
        }
        bool ReadNextFrame(AudioFrame& frame);
        void SeekToTimestamp(uint64_t timestamp);

    private:
        std::string filePath;
        std::unique_ptr<StdIOCallback> ioCallback;
        std::unique_ptr<EbmlStream> ebmlStream;

        SegmentInfo segmentInfo;
        std::vector<AudioTrackInfo> audioTracks;

        EbmlElement* level0 = nullptr;
        EbmlElement* level1 = nullptr;
        EbmlElement* level2 = nullptr;

        int upperLevel = 0;

        void ParseEbmlHead(EbmlHead* head);
        void ParseSegment(KaxSegment* segment);
        void ParseInfo(KaxInfo* info);
        void ParseTracks(KaxTracks* tracks);
        void ParseTrackEntry(KaxTrackEntry* entry);
        void ParseCluster(KaxCluster* cluster);

        void Reset();
    };
}  // namespace SF::Engine

#endif  // MATROSKA_AUDIO_LOADER_HPP