#ifndef MATROSKA_AUDIO_LOADER_HPP
#define MATROSKA_AUDIO_LOADER_HPP

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <deque>

// EBML includes
#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/StdIOCallback.h>

// Matroska includes
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>

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

        KaxSegment* segmentElement = nullptr;   // non-owning alias into level0; level0 still owns/deletes it
        filepos_t   segmentDataStartPos = 0;    // file offset of the first byte inside the segment payload
        filepos_t   clusterScanPos = 0;         // resume point for the next cluster scan
        std::deque<AudioFrame> pendingFrames;   // frames extracted from the most recently loaded cluster

        void ParseEbmlHead(EbmlHead* head);
        void ParseSegment(KaxSegment* segment);
        void ParseInfo(KaxInfo* info);
        void ParseTracks(KaxTracks* tracks);
        void ParseTrackEntry(KaxTrackEntry* entry);
        void ParseCluster(KaxCluster* cluster);

        bool LoadNextClusterFrames();   // scans forward for the next KaxCluster, fills pendingFrames
        void ExtractFramesFromBlock(KaxInternalBlock& block, bool keyframe, std::deque<AudioFrame>& out);
        bool IsAudioTrack(uint64_t trackNumber) const;

        void Reset();
    };
}  // namespace SF::Engine

#endif  // MATROSKA_AUDIO_LOADER_HPP