#include "MatroskaAudioLoader.hpp"
#include <iomanip>

#include <matroska/KaxContexts.h>

namespace SF::Engine
{
    MatroskaAudioLoader::MatroskaAudioLoader(const std::string& filePath) : filePath(filePath) {}

    MatroskaAudioLoader::~MatroskaAudioLoader()
    {
        Reset();
    }

    void MatroskaAudioLoader::ParseEbmlHead(EbmlHead* head)
    {
        head->Read(*ebmlStream, EBML_CONTEXT(head), upperLevel, level1, true);

        EbmlElement* elem = nullptr;
        int upperLvl = 0;

        elem = ebmlStream->FindNextElement(EBML_CONTEXT(head), upperLvl, 0xFFFFFFFFL, true);
        while (elem)
        {
            if (upperLvl > 0)
            {
                break;
            }

            if (EbmlId(*elem) == EBML_ID(EDocType))
            {
                EDocType& docType = *static_cast<EDocType*>(elem);
                docType.ReadData(ebmlStream->I_O());
            }
            else if (EbmlId(*elem) == EBML_ID(EDocTypeVersion))
            {
                EDocTypeVersion& version = *static_cast<EDocTypeVersion*>(elem);
                version.ReadData(ebmlStream->I_O());
            }

            delete elem;
            elem = ebmlStream->FindNextElement(EBML_CONTEXT(head), upperLvl, 0xFFFFFFFFL, true);
        }

        if (elem)
        {
            delete elem;
        }
    }

    void MatroskaAudioLoader::ParseSegment(KaxSegment* segment)
    {
        // Read segment content
        EbmlElement* l1 =
            ebmlStream->FindNextElement(EBML_CONTEXT(segment), upperLevel, 0xFFFFFFFFL, true);

        while (l1)
        {
            if (upperLevel > 0)
            {
                break;
            }

            if (EbmlId(*l1) == EBML_ID(KaxInfo))
            {
                KaxInfo* info = static_cast<KaxInfo*>(l1);
                ParseInfo(info);
            }
            else if (EbmlId(*l1) == EBML_ID(KaxTracks))
            {
                KaxTracks* tracks = static_cast<KaxTracks*>(l1);
                ParseTracks(tracks);
            }
            else if (EbmlId(*l1) == EBML_ID(KaxCluster))
            {
                l1->SkipData(*ebmlStream, EBML_CONTEXT(l1));
            }
            else if (EbmlId(*l1) == EBML_ID(KaxSeekHead))
            {
                l1->SkipData(*ebmlStream, EBML_CONTEXT(l1));
            }
            else if (EbmlId(*l1) == EBML_ID(KaxCues))
            {
                l1->SkipData(*ebmlStream, EBML_CONTEXT(l1));
            }
            else
            {
                l1->SkipData(*ebmlStream, EBML_CONTEXT(l1));
            }

            delete l1;
            l1 = ebmlStream->FindNextElement(EBML_CONTEXT(segment), upperLevel, 0xFFFFFFFFL, true);
        }

        if (l1)
        {
            delete l1;
        }
    }

    void MatroskaAudioLoader::ParseInfo(KaxInfo* info)
    {
        int upperLvl = 0;
        info->Read(*ebmlStream, EBML_CONTEXT(info), upperLvl, level2, true);

        for (size_t i = 0; i < info->ListSize(); i++)
        {
            EbmlElement* elem = (*info)[i];

            // Get the element's ID name for debugging
            std::string elemName = typeid(*elem).name();

            if (dynamic_cast<EbmlUInteger*>(elem))
            {
                // Likely timecode scale
                EbmlUInteger* val = static_cast<EbmlUInteger*>(elem);
                segmentInfo.timecodeScale = val->GetValue();
            }
            else if (dynamic_cast<EbmlFloat*>(elem))
            {
                // Likely duration
                EbmlFloat* val = static_cast<EbmlFloat*>(elem);
                segmentInfo.duration = val->GetValue();
            }
            else if (dynamic_cast<EbmlUnicodeString*>(elem))
            {
                EbmlUnicodeString* str = static_cast<EbmlUnicodeString*>(elem);
            }
        }
    }

    void MatroskaAudioLoader::ParseTracks(KaxTracks* tracks)
    {
        int upperLvl = 0;
        tracks->Read(*ebmlStream, EBML_CONTEXT(tracks), upperLvl, level2, true);

        EbmlElement* elem = nullptr;
        elem = ebmlStream->FindNextElement(EBML_CONTEXT(tracks), upperLvl, 0xFFFFFFFFL, true);

        while (elem)
        {
            if (upperLvl > 0)
            {
                break;
            }

            if (EbmlId(*elem) == EBML_ID(KaxTrackEntry))
            {
                KaxTrackEntry* entry = static_cast<KaxTrackEntry*>(elem);
                ParseTrackEntry(entry);
            }

            delete elem;
            elem = ebmlStream->FindNextElement(EBML_CONTEXT(tracks), upperLvl, 0xFFFFFFFFL, true);
        }

        if (elem)
        {
            delete elem;
        }
    }

    void MatroskaAudioLoader::ParseTrackEntry(KaxTrackEntry* entry)
    {
        int upperLvl = 0;
        entry->Read(*ebmlStream, EBML_CONTEXT(entry), upperLvl, level2, true);

        AudioTrackInfo trackInfo = {};
        trackInfo.enabled = true;
        trackInfo.isDefault = false;
        trackInfo.samplingFrequency = 48000.0;  // Default
        trackInfo.channels = 2;                 // Default
        trackInfo.bitDepth = 16;                // Default

        EbmlElement* elem = nullptr;
        elem = ebmlStream->FindNextElement(EBML_CONTEXT(entry), upperLvl, 0xFFFFFFFFL, true);

        bool isAudioTrack = false;

        while (elem)
        {
            if (upperLvl > 0)
            {
                break;
            }

            if (EbmlId(*elem) == EBML_ID(KaxTrackNumber))
            {
                KaxTrackNumber& num = *static_cast<KaxTrackNumber*>(elem);
                num.ReadData(ebmlStream->I_O());
                trackInfo.trackNumber = uint64(num);
            }
            else if (EbmlId(*elem) == EBML_ID(KaxTrackType))
            {
                KaxTrackType& type = *static_cast<KaxTrackType*>(elem);
                type.ReadData(ebmlStream->I_O());
                isAudioTrack = (uint64(type) == track_audio);
            }
            else if (EbmlId(*elem) == EBML_ID(KaxCodecID))
            {
                KaxCodecID& codec = *static_cast<KaxCodecID*>(elem);
                codec.ReadData(ebmlStream->I_O());
                trackInfo.codecID = std::string(codec);
            }
            else if (EbmlId(*elem) == EBML_ID(KaxCodecName))
            {
                KaxCodecName& name = *static_cast<KaxCodecName*>(elem);
                name.ReadData(ebmlStream->I_O());
                trackInfo.codecName = UTFstring(name).GetUTF8();
            }
            else if (EbmlId(*elem) == EBML_ID(KaxTrackLanguage))
            {
                KaxTrackLanguage& lang = *static_cast<KaxTrackLanguage*>(elem);
                lang.ReadData(ebmlStream->I_O());
                trackInfo.language = std::string(lang);
            }
            else if (EbmlId(*elem) == EBML_ID(KaxCodecPrivate))
            {
                KaxCodecPrivate& priv = *static_cast<KaxCodecPrivate*>(elem);
                priv.ReadData(ebmlStream->I_O());
                trackInfo.codecPrivate.resize(priv.GetSize());
                memcpy(trackInfo.codecPrivate.data(), priv.GetBuffer(), priv.GetSize());
            }
            else if (EbmlId(*elem) == EBML_ID(KaxTrackFlagEnabled))
            {
                KaxTrackFlagEnabled& flag = *static_cast<KaxTrackFlagEnabled*>(elem);
                flag.ReadData(ebmlStream->I_O());
                trackInfo.enabled = uint64(flag) != 0;
            }
            else if (EbmlId(*elem) == EBML_ID(KaxTrackFlagDefault))
            {
                KaxTrackFlagDefault& flag = *static_cast<KaxTrackFlagDefault*>(elem);
                flag.ReadData(ebmlStream->I_O());
                trackInfo.isDefault = uint64(flag) != 0;
            }
            else if (EbmlId(*elem) == EBML_ID(KaxTrackAudio))
            {
                // Parse audio-specific data
                KaxTrackAudio* audio = static_cast<KaxTrackAudio*>(elem);
                int audioUpperLvl = 0;
                audio->Read(*ebmlStream, EBML_CONTEXT(audio), audioUpperLvl, level2, true);

                EbmlElement* audioElem = ebmlStream->FindNextElement(
                    EBML_CONTEXT(audio), audioUpperLvl, 0xFFFFFFFFL, true);

                while (audioElem)
                {
                    if (audioUpperLvl > 0)
                    {
                        break;
                    }

                    if (EbmlId(*audioElem) == EBML_ID(KaxAudioSamplingFreq))
                    {
                        KaxAudioSamplingFreq& freq = *static_cast<KaxAudioSamplingFreq*>(audioElem);
                        freq.ReadData(ebmlStream->I_O());
                        trackInfo.samplingFrequency = double(freq);
                    }
                    else if (EbmlId(*audioElem) == EBML_ID(KaxAudioChannels))
                    {
                        KaxAudioChannels& ch = *static_cast<KaxAudioChannels*>(audioElem);
                        ch.ReadData(ebmlStream->I_O());
                        trackInfo.channels = uint64(ch);
                    }
                    else if (EbmlId(*audioElem) == EBML_ID(KaxAudioBitDepth))
                    {
                        KaxAudioBitDepth& depth = *static_cast<KaxAudioBitDepth*>(audioElem);
                        depth.ReadData(ebmlStream->I_O());
                        trackInfo.bitDepth = uint64(depth);
                    }

                    delete audioElem;
                    audioElem = ebmlStream->FindNextElement(EBML_CONTEXT(audio), audioUpperLvl,
                                                            0xFFFFFFFFL, true);
                }

                if (audioElem)
                {
                    delete audioElem;
                }
            }

            delete elem;
            elem = ebmlStream->FindNextElement(EBML_CONTEXT(entry), upperLvl, 0xFFFFFFFFL, true);
        }

        if (elem)
        {
            delete elem;
        }

        // Only add audio tracks
        if (isAudioTrack)
        {
            audioTracks.push_back(trackInfo);
        }
    }

    void MatroskaAudioLoader::PrintInfo()
    {
        std::cout << "\n=== Summary ===" << "\n";
        std::cout << "Total Audio Tracks: " << audioTracks.size() << "\n";

        if (!segmentInfo.title.empty())
        {
            std::cout << "Title: " << segmentInfo.title << "\n";
        }

        if (segmentInfo.duration > 0)
        {
            std::cout << "Duration: " << std::fixed << std::setprecision(2)
                      << segmentInfo.duration / 1000.0 << " seconds" << "\n";
        }
    }
    void MatroskaAudioLoader::Reset()
    {
        pendingFrames.clear();
        segmentElement = nullptr;
        segmentDataStartPos = 0;
        clusterScanPos = 0;

        if (level2) { delete level2; level2 = nullptr; }
        if (level1) { delete level1; level1 = nullptr; }
        if (level0) { delete level0; level0 = nullptr; }
    }

    bool MatroskaAudioLoader::Load()
    {
        try
        {
            ioCallback = std::make_unique<StdIOCallback>(filePath.c_str(), MODE_READ);
            ebmlStream = std::make_unique<EbmlStream>(*ioCallback);

            level0 = ebmlStream->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFL);
            if (!level0) return false;

            EbmlHead* head = static_cast<EbmlHead*>(level0);
            ParseEbmlHead(head);

            level0 = ebmlStream->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFL);
            if (!level0) return false;

            KaxSegment* segment = static_cast<KaxSegment*>(level0);

            // Capture these BEFORE ParseSegment consumes the stream, so ReadNextFrame/
            // SeekToTimestamp can independently re-scan the segment for clusters later.
            segmentElement = segment;
            segmentDataStartPos = ebmlStream->I_O().getFilePointer();
            clusterScanPos = segmentDataStartPos;

            ParseSegment(segment);

            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }

    bool MatroskaAudioLoader::IsAudioTrack(uint64_t trackNumber) const
    {
        for (const auto& t : audioTracks)
        {
            if (t.trackNumber == trackNumber)
                return true;
        }
        return false;
    }

    void MatroskaAudioLoader::ExtractFramesFromBlock(KaxInternalBlock& block, bool keyframe,
                                                    std::deque<AudioFrame>& out)
    {
        uint64_t trackNum = block.TrackNum();
        if (!IsAudioTrack(trackNum))
            return; // ignore video/subtitle blocks entirely

        uint64_t globalTc = block.GlobalTimecode();

        for (unsigned int i = 0; i < block.NumberFrames(); ++i)
        {
            DataBuffer& buf = block.GetBuffer(i);

            AudioFrame frame;
            frame.trackNumber = trackNum;
            frame.timestamp = globalTc;
            frame.keyframe = keyframe;
            frame.data.assign(reinterpret_cast<const uint8_t*>(buf.Buffer()),
                            reinterpret_cast<const uint8_t*>(buf.Buffer()) + buf.Size());

            out.push_back(std::move(frame));
        }
    }

    bool MatroskaAudioLoader::LoadNextClusterFrames()
    {
        if (!segmentElement)
            return false;

        ebmlStream->I_O().setFilePointer(clusterScanPos);

        int segUpperLvl = 0;
        EbmlElement* el = ebmlStream->FindNextElement(EBML_CONTEXT(segmentElement), segUpperLvl,
                                                    0xFFFFFFFFL, true);

        // Skip anything that isn't a cluster (SeekHead, Cues, another Info block, etc.)
        while (el && EbmlId(*el) != EBML_ID(KaxCluster))
        {
            if (segUpperLvl > 0) { delete el; return false; } // walked past the segment
            el->SkipData(*ebmlStream, EBML_CONTEXT(el));
            delete el;
            el = ebmlStream->FindNextElement(EBML_CONTEXT(segmentElement), segUpperLvl,
                                            0xFFFFFFFFL, true);
        }

        if (!el || segUpperLvl > 0)
        {
            if (el) delete el;
            return false; // no more clusters -- end of stream
        }

        KaxCluster* cluster = static_cast<KaxCluster*>(el);

        int clUpperLvl = 0;
        EbmlElement* cel = ebmlStream->FindNextElement(EBML_CONTEXT(cluster), clUpperLvl,
                                                        0xFFFFFFFFL, true);
        while (cel)
        {
            if (clUpperLvl > 0) { delete cel; break; }

            if (EbmlId(*cel) == EBML_ID(KaxClusterTimecode))
            {
                KaxClusterTimecode& tc = *static_cast<KaxClusterTimecode*>(cel);
                tc.ReadData(ebmlStream->I_O());
                cluster->InitTimecode(uint64(tc), segmentInfo.timecodeScale);
            }
            else if (EbmlId(*cel) == EBML_ID(KaxSimpleBlock))
            {
                KaxSimpleBlock& sb = *static_cast<KaxSimpleBlock*>(cel);
                sb.ReadData(ebmlStream->I_O());
                sb.SetParent(*cluster);
                ExtractFramesFromBlock(sb, sb.IsKeyframe(), pendingFrames);
            }
            else if (EbmlId(*cel) == EBML_ID(KaxBlockGroup))
            {
                KaxBlockGroup* bg = static_cast<KaxBlockGroup*>(cel);
                EbmlElement* bgFoundElt = nullptr;
                int bgUpperLvl = 0;
                bg->Read(*ebmlStream, EBML_CONTEXT(bg), bgUpperLvl, bgFoundElt, true);

                KaxBlock* blk = FindChild<KaxBlock>(*bg);
                if (blk)
                {
                    // Standard Matroska convention: a block with no ReferenceBlock child
                    // depends on nothing else, i.e. it's a keyframe.
                    bool isKey = (bg->FindElt(EBML_INFO(KaxReferenceBlock)) == nullptr);
                    blk->SetParent(*cluster);
                    ExtractFramesFromBlock(*blk, isKey, pendingFrames);
                }
            }
            else
            {
                cel->SkipData(*ebmlStream, EBML_CONTEXT(cel));
            }

            delete cel;
            cel = ebmlStream->FindNextElement(EBML_CONTEXT(cluster), clUpperLvl, 0xFFFFFFFFL, true);
        }

        clusterScanPos = ebmlStream->I_O().getFilePointer();
        delete cluster;

        return true;
    }

    bool MatroskaAudioLoader::ReadNextFrame(AudioFrame& frame)
    {
        while (pendingFrames.empty())
        {
            if (!LoadNextClusterFrames())
                return false; // genuinely out of clusters
        }

        frame = std::move(pendingFrames.front());
        pendingFrames.pop_front();
        return true;
    }

    void MatroskaAudioLoader::SeekToTimestamp(uint64_t timestamp)
    {
        if (!segmentElement)
            return;
        pendingFrames.clear();
        clusterScanPos = segmentDataStartPos;

        while (true)
        {
            if (!LoadNextClusterFrames())
                return; // reached EOF without finding the timestamp; stream left at end

            if (pendingFrames.empty())
                continue; // cluster had no audio-track frames, keep scanning

            if (pendingFrames.front().timestamp >= timestamp)
                return; // ready, next ReadNextFrame() starts here

            pendingFrames.clear(); // whole cluster was before the target, discard and continue
        }
    }
}  // namespace SF::Engine