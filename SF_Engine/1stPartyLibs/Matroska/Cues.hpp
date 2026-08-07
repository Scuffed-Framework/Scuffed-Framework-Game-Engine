#pragma once

#include "../EBML/Element.hpp"
#include "MatroskaIds.hpp"
#include <vector>
#include <map>
#include <algorithm>
#include <optional>

using namespace SF::EBML;
namespace SF::Matroska
{
    struct CuePoint
    {
        uint64_t timecode = 0;
        struct TrackPosition
        {
            uint64_t trackNumber = 0;
            uint64_t clusterPosition = 0;
            uint64_t blockNumber = 0;
            uint64_t codecState = 0;
            uint64_t reference = 0;
            uint64_t relativePosition = 0;
            uint64_t duration = 0;
        };
        std::vector<TrackPosition> positions;
    };

    // Generate Cues with proper sorting
    inline Element generate_cues(const std::vector<CuePoint> &cuePoints)
    {
        auto cues = Element::make_master(Matroska::ids::Cues);

        // Sort cue points by timecode for better seeking
        auto sortedPoints = cuePoints;
        std::sort(sortedPoints.begin(), sortedPoints.end(),
            [](const CuePoint& a, const CuePoint& b) {
                return a.timecode < b.timecode;
            });

        for (const auto &point : sortedPoints)
        {
            auto cuePoint = Element::make_master(Matroska::ids::CuePoint);
            cuePoint.add(Element::make_uint(Matroska::ids::CueTime, point.timecode));

            for (const auto &pos : point.positions)
            {
                auto trackPos = Element::make_master(Matroska::ids::CueTrackPositions);
                trackPos.add(Element::make_uint(Matroska::ids::CueTrack, pos.trackNumber));
                trackPos.add(Element::make_uint(Matroska::ids::CueClusterPosition, pos.clusterPosition));
                
                if (pos.relativePosition)
                    trackPos.add(Element::make_uint(Matroska::ids::CueRelativePosition, pos.relativePosition));
                if (pos.blockNumber)
                    trackPos.add(Element::make_uint(Matroska::ids::CueBlockNumber, pos.blockNumber));
                if (pos.codecState)
                    trackPos.add(Element::make_uint(Matroska::ids::CueCodecState, pos.codecState));
                if (pos.reference)
                    trackPos.add(Element::make_uint(Matroska::ids::CueReference, pos.reference));
                if (pos.duration)
                    trackPos.add(Element::make_uint(Matroska::ids::CueDuration, pos.duration));
                
                cuePoint.add(std::move(trackPos));
            }
            cues.add(std::move(cuePoint));
        }

        return cues;
    }

    // Generate SeekHead with proper positions. Each position is optional:
    // pass std::nullopt (the default) to omit that entry rather than 0, since
    // 0 is a legitimate real position (e.g. Info is always the first child
    // of Segment, so its position is 0) and can't be used as a sentinel.
    inline Element generate_seek_head(
        std::optional<uint64_t> infoPosition,
        std::optional<uint64_t> tracksPosition,
        std::optional<uint64_t> cuesPosition,
        std::optional<uint64_t> chaptersPosition = std::nullopt,
        std::optional<uint64_t> tagsPosition = std::nullopt)
    {
        auto seekHead = Element::make_master(Matroska::ids::SeekHead);

        auto add_seek = [&](uint32_t id, std::optional<uint64_t> position)
        {
            if (!position)
                return;
            auto seek = Element::make_master(Matroska::ids::Seek);
            std::vector<byte> idBytes = Identifier(id).encode();
            seek.add(Element::make_binary(Matroska::ids::SeekID, std::move(idBytes)));
            seek.add(Element::make_uint(Matroska::ids::SeekPosition, *position));
            seekHead.add(std::move(seek));
        };

        add_seek(0x1549A966, infoPosition);
        add_seek(0x1654AE6B, tracksPosition);
        add_seek(0x1C53BB6B, cuesPosition);
        add_seek(0x1043A770, chaptersPosition);
        add_seek(0x1254C367, tagsPosition);

        return seekHead;
    }

    // Helper to find cue points for a specific track
    inline std::vector<CuePoint> filter_cue_points_by_track(
        const std::vector<CuePoint>& cuePoints,
        uint64_t trackNumber)
    {
        std::vector<CuePoint> result;
        for (const auto& point : cuePoints)
        {
            for (const auto& pos : point.positions)
            {
                if (pos.trackNumber == trackNumber)
                {
                    result.push_back(point);
                    break;
                }
            }
        }
        return result;
    }
}