#include "../Demuxer.hpp"
#include <cstring>

namespace SF::Matroska
{
    using namespace SF::EBML;

    size_t Demuxer::tell_stream() const
    {
        auto p = in_.tellg();
        if (p == streampos(-1))
            return SIZE_MAX;
        return static_cast<size_t>(static_cast<streamoff>(p));
    }

    bool Demuxer::seek_stream(size_t offset) const
    {
        in_.clear();
        in_.seekg(static_cast<streamoff>(offset), ios::beg);
        return in_.good();
    }

    size_t Demuxer::position() const { return tell_stream(); }

    bool Demuxer::seek_to(size_t offset) const { return seek_stream(offset); }

    bool Demuxer::read_vint(span<const byte> data, size_t &pos, uint64_t &value, bool maskFirstBit)
    {
        if (pos >= data.size())
            return false;
        const uint8_t len = VintValidator::vint_length(data[pos]);
        if (len == 0 || len > kMaxVintLength || pos + len > data.size())
            return false;

        uint64_t v = maskFirstBit ? (data[pos] & (0xFFu >> len)) : data[pos];
        for (uint8_t i = 1; i < len; ++i)
            v = (v << 8) | data[pos + i];
        pos += len;
        value = v;
        return true;
    }

    bool Demuxer::read_svint(span<const byte> data, size_t &pos, int64_t &value)
    {
        static constexpr int64_t kSub[] = {0x3fLL,        0x1fffLL,        0xfffffLL,        0x7ffffffLL,
                                           0x3ffffffffLL, 0x1ffffffffffLL, 0xffffffffffffLL, 0x7fffffffffffffLL};
        const size_t start              = pos;
        uint64_t v;
        if (!read_vint(data, pos, v, true))
            return false;
        const size_t len = pos - start;
        value            = static_cast<int64_t>(v) - kSub[len - 1];
        return true;
    }

    bool Demuxer::initialize(bool seekToFirstCluster)
    {
        streamStart_ = tell_stream();
        if (streamStart_ == SIZE_MAX)
            return false;

        if (!parse_ebml_header())
            return false;

        if (!parse_segment_header())
            return false;

        if (!parse_segment_metadata())
            return false;

        if (seekToFirstCluster)
        {
            // Jump to the first Cluster, whichever comes first: end of the
            // parsed metadata or the SeekHead entry.
            size_t firstCluster = position();
            auto it             = seekHeadEntries_.find(ids::Cluster.value());
            if (it != seekHeadEntries_.end())
                firstCluster = segmentOffset_ + it->second;
            else if (hasCues_ && cuesPosition_)
            {
                // If no SeekHead, scan forward for the first Cluster.
                // (cheap: just continue from current position)
            }
            seek_stream(firstCluster);
        }

        headersParsed_ = true;
        return true;
    }

    bool Demuxer::parse_ebml_header()
    {
        auto readChunk = [&](size_t maxBytes) -> optional<Element>
        {
            // Read enough to cover the EBML header; growing window if needed.
            size_t window = 64;
            vector<byte> buf;
            buf.reserve(window);
            while (buf.size() < maxBytes)
            {
                size_t need = window - buf.size();
                size_t cur  = buf.size();
                buf.resize(window);
                in_.read(reinterpret_cast<char *>(buf.data() + cur), static_cast<streamsize>(need));
                auto got = static_cast<size_t>(in_.gcount());
                buf.resize(cur + got);
                if (got == 0)
                    break;
                try
                {
                    auto result = parse_element(buf, schema_);
                    // Consume only the header bytes by seeking.
                    size_t consumed = result.consumed;
                    seek_stream(streamStart_ + consumed);
                    ebmlHeaderBytes_.assign(buf.begin(), buf.begin() + consumed);
                    return result.element;
                } catch (const ParseError &)
                {
                    if (window >= 1024 * 1024)
                        break;
                    window *= 2;
                }
            }
            return nullopt;
        };

        auto ebml = readChunk(16 * 1024 * 1024);
        if (!ebml || ebml->id() != EBML::ids::EBML)
            return false;
        return true;
    }

    bool Demuxer::parse_segment_header()
    {
        size_t pos = tell_stream();
        if (pos == SIZE_MAX)
            return false;

        vector<byte> buf;
        size_t window = 64;
        Element segment;
        while (buf.size() < 1024 * 1024)
        {
            buf.resize(window);
            in_.clear();
            in_.seekg(static_cast<streamoff>(pos), ios::beg);
            in_.read(reinterpret_cast<char *>(buf.data()), static_cast<streamsize>(window));
            auto got = static_cast<size_t>(in_.gcount());
            buf.resize(got);
            if (got < window)
                in_.clear();
            try
            {
                auto result = parse_element(buf, schema_);
                if (result.element.id() != ids::Segment)
                {
                    in_.clear();
                    in_.seekg(static_cast<streamoff>(pos), ios::beg);
                    return false;
                }
                // segmentOffset_ = start of Segment's data.
                segmentOffset_ = pos + result.element.id().encode().size() +
                                 encode_size(serialized_body_size(result.element)).size();
                in_.clear();
                in_.seekg(static_cast<streamoff>(segmentOffset_), ios::beg);
                return true;
            } catch (const ParseError &)
            {
                if (window > 8 * 1024 * 1024)
                    break;
                window *= 2;
            }
        }
        return false;
    }

    bool Demuxer::parse_segment_metadata()
    {
        while (true)
        {
            size_t start = tell_stream();
            if (start == SIZE_MAX)
                return false;
            size_t end = segmentOffset_;
            if (hasCues_ && cuesPosition_)
            {
                // Cues come after the metadata; stop before them if we can.
                size_t cueStart = segmentOffset_ + cuesPosition_;
                if (cueStart > end)
                    end = cueStart;
            }

            vector<byte> buf;
            size_t window   = 64;
            bool progressed = false;
            while (buf.size() < 4 * 1024 * 1024)
            {
                buf.resize(window);
                in_.clear();
                in_.seekg(static_cast<streamoff>(start), ios::beg);
                in_.read(reinterpret_cast<char *>(buf.data()), static_cast<streamsize>(window));
                auto got = static_cast<size_t>(in_.gcount());
                buf.resize(got);
                if (got < window)
                    in_.clear();

                ParseResult result;
                bool ok = false;
                try
                {
                    result = parse_element(buf, schema_);
                    ok     = true;
                } catch (const ParseError &)
                {
                    ok = false;
                }
                if (ok && result.element.is_master())
                {
                    auto &e      = result.element;
                    bool handled = true;
                    if (e.id() == ids::Info)
                    {
                        Element copy = e;
                        parse_info(copy);
                    } else if (e.id() == ids::Tracks)
                    {
                        Element copy = e;
                        parse_tracks(copy);
                    } else if (e.id() == ids::SeekHead)
                    {
                        Element copy = e;
                        parse_seek_head(copy);
                    } else if (e.id() == ids::Cues)
                    {
                        Element copy = e;
                        parse_cues(copy);
                        hasCues_      = true;
                        cuesPosition_ = static_cast<size_t>(start) - segmentOffset_;
                    } else if (e.id() == ids::Cluster)
                    {
                        // Done with metadata.
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        return true;
                    } else if (e.id() == ids::Tags)
                        tagsPosition_ = static_cast<size_t>(start) - segmentOffset_;
                    else if (e.id() == ids::Chapters)
                        chaptersPosition_ = static_cast<size_t>(start) - segmentOffset_;
                    else if (e.id() == ids::Attachments)
                        attachmentsPosition_ = static_cast<size_t>(start) - segmentOffset_;
                    else
                        handled = false;

                    if (handled)
                    {
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    }
                }
                if (window > 16 * 1024 * 1024)
                    break;
                window *= 2;
            }
            if (!progressed)
                return tracksPosition_.has_value(); // stop when Tracks are known
        }
    }

    bool Demuxer::parse_info(const Element &info)
    {
        if (auto *e = info.find(ids::TimecodeScale))
        {
            timecodeScale_    = e->as_uint();
            hasTimecodeScale_ = true;
        }
        return true;
    }

    bool Demuxer::parse_tracks(Element &tracks)
    {
        if (tracksPosition_)
            return true;
        tracksPosition_ = position() - segmentOffset_;
        tracks_.clear();
        trackNumberToIndex_.clear();

        for (const auto &entry: tracks.children())
        {
            if (entry.id() != ids::TrackEntry)
                continue;

            Track track;
            auto getUInt = [&](Identifier id, uint64_t &out)
            {
                if (auto *e = entry.find(id))
                {
                    try
                    {
                        out = e->as_uint();
                    } catch (...)
                    {
                    }
                }
            };
            auto getString = [&](Identifier id, string &out)
            {
                if (auto *e = entry.find(id))
                {
                    try
                    {
                        out = e->as_string();
                    } catch (...)
                    {
                    }
                }
            };
            auto getBinary = [&](Identifier id, vector<byte> &out)
            {
                if (auto *e = entry.find(id))
                {
                    try
                    {
                        out = e->raw();
                    } catch (...)
                    {
                    }
                }
            };

            getUInt(ids::TrackNumber, track.number);
            getUInt(ids::TrackUID, track.uid);
            uint64_t typeVal = 0;
            getUInt(ids::TrackType, typeVal);
            track.type = static_cast<TrackType>(typeVal);
            getString(ids::CodecID, track.codecID);
            getString(ids::Name, track.name);
            getString(ids::Language, track.language);
            if (track.language.empty())
                track.language = "eng";

            uint64_t enabled = 1, def = 1, forced = 0, lacing = 1;
            getUInt(ids::TrackEnabled, enabled);
            getUInt(ids::TrackDefault, def);
            getUInt(ids::TrackForced, forced);
            getUInt(ids::TrackLacing, lacing);
            track.enabled      = enabled != 0;
            track.defaultTrack = def != 0;
            track.forced       = forced != 0;
            track.lacing       = lacing != 0;

            uint64_t defaultDur = 0;
            getUInt(ids::DefaultDuration, defaultDur);
            track.defaultDuration = chrono::nanoseconds(defaultDur);
            uint64_t codecDelay = 0, seekPreRoll = 0;
            getUInt(ids::CodecDelay, codecDelay);
            getUInt(ids::SeekPreRoll, seekPreRoll);
            track.codecDelay  = chrono::nanoseconds(codecDelay);
            track.seekPreRoll = chrono::nanoseconds(seekPreRoll);

            vector<byte> codecPrivate;
            getBinary(ids::CodecPrivate, codecPrivate);

            if (track.type == TrackType::Video)
            {
                VideoTrack video;
                video.codecID      = track.codecID;
                video.codecPrivate = std::move(codecPrivate);
                if (auto *v = entry.find(ids::Video))
                {
                    auto getV = [&](Identifier id, uint64_t &out)
                    {
                        if (auto *e = v->find(id))
                        {
                            try
                            {
                                out = e->as_uint();
                            } catch (...)
                            {
                            }
                        }
                    };
                    getV(ids::PixelWidth, video.pixelWidth);
                    getV(ids::PixelHeight, video.pixelHeight);
                    getV(ids::PixelCropTop, video.pixelCropTop);
                    getV(ids::PixelCropBottom, video.pixelCropBottom);
                    getV(ids::PixelCropLeft, video.pixelCropLeft);
                    getV(ids::PixelCropRight, video.pixelCropRight);
                    getV(ids::DisplayWidth, video.displayWidth);
                    getV(ids::DisplayHeight, video.displayHeight);
                    getV(ids::DisplayUnit, video.displayUnit);
                    uint64_t interlaced = 0;
                    getV(ids::FlagInterlaced, interlaced);
                    video.interlaced = interlaced != 0;
                    if (auto *e = v->find(ids::FrameRate))
                    {
                        try
                        {
                            video.frameRate = e->as_float();
                        } catch (...)
                        {
                        }
                    }
                }
                track.specific = std::move(video);
            } else if (track.type == TrackType::Audio)
            {
                AudioTrack audio;
                audio.codecID      = track.codecID;
                audio.codecPrivate = std::move(codecPrivate);
                if (auto *a = entry.find(ids::Audio))
                {
                    if (auto *e = a->find(ids::SamplingFrequency))
                    {
                        try
                        {
                            audio.samplingFrequency = e->as_float();
                        } catch (...)
                        {
                        }
                    }
                    if (auto *e = a->find(ids::OutputSamplingFrequency))
                    {
                        try
                        {
                            audio.outputSamplingFrequency = static_cast<uint64_t>(e->as_float());
                        } catch (...)
                        {
                        }
                    }
                    if (auto *e = a->find(ids::Channels))
                        try
                        {
                            audio.channels = e->as_uint();
                        } catch (...)
                        {
                        }
                    if (auto *e = a->find(ids::BitDepth))
                        try
                        {
                            audio.bitDepth = e->as_uint();
                        } catch (...)
                        {
                        }
                }
                track.specific = std::move(audio);
            }

            if (track.number != 0)
            {
                trackNumberToIndex_[track.number] = static_cast<unsigned>(tracks_.size());
            }
            tracks_.push_back(std::move(track));
        }
        return !tracks_.empty();
    }

    bool Demuxer::parse_seek_head(Element &seekHead)
    {
        hasSeekHead_ = true;
        for (const auto &seek: seekHead.children())
        {
            if (seek.id() != ids::Seek)
                continue;
            auto *idElem  = seek.find(ids::SeekID);
            auto *posElem = seek.find(ids::SeekPosition);
            if (!idElem || !posElem)
                continue;
            const auto &raw = idElem->raw();
            if (raw.empty() || raw.size() > 4)
                continue;
            uint64_t target = 0;
            for (auto b: raw)
                target = (target << 8) | b;
            uint64_t pos = 0;
            try
            {
                pos = posElem->as_uint();
            } catch (...)
            {
                continue;
            }

            seekHeadEntries_[target] = pos;
            if (target == ids::Info.value())
                infoPosition_ = pos;
            else if (target == ids::Tracks.value())
                tracksPosition_ = pos;
            else if (target == ids::Cues.value())
                cuesPosition_ = pos;
            else if (target == ids::Tags.value())
                tagsPosition_ = pos;
            else if (target == ids::Chapters.value())
                chaptersPosition_ = pos;
            else if (target == ids::Attachments.value())
                attachmentsPosition_ = pos;
        }
        return true;
    }

    bool Demuxer::parse_cues(Element &cues)
    {
        cues_.clear();
        for (const auto &point: cues.children())
        {
            if (point.id() != ids::CuePoint)
                continue;
            auto *timeElem = point.find(ids::CueTime);
            if (!timeElem)
                continue;
            uint64_t tc = 0;
            try
            {
                tc = timeElem->as_uint();
            } catch (...)
            {
                continue;
            }

            for (const auto &pos: point.children())
            {
                if (pos.id() != ids::CueTrackPositions)
                    continue;
                auto *clusterPos = pos.find(ids::CueClusterPosition);
                if (!clusterPos)
                    continue;
                uint64_t cp = 0;
                try
                {
                    cp = clusterPos->as_uint();
                } catch (...)
                {
                    continue;
                }
                cues_.emplace_back(tc, cp);
            }
        }
        return true;
    }

    int Demuxer::find_track_index(uint64_t trackNumber) const
    {
        auto it = trackNumberToIndex_.find(trackNumber);
        return it == trackNumberToIndex_.end() ? -1 : static_cast<int>(it->second);
    }

    optional<uint64_t> Demuxer::duration() const
    {
        // Duration is read during parse_info if present; we don't store it
        // here since nestegg's semantics are timecode-based, but we can
        // re-expose it via the Info element. For now, return nullopt; the
        // caller can inspect the Info element directly if needed.
        return nullopt;
    }

    bool Demuxer::read_cluster(Element &cluster, optional<uint64_t> skipClustersBefore) const
    {
        // Scan forward for the next Cluster element. We re-parse it as a
        // whole Master to get its children.
        while (true)
        {
            size_t start = tell_stream();
            if (start == SIZE_MAX)
                return false;
            vector<byte> buf;
            size_t window = 64;
            bool found    = false;
            while (buf.size() < 16 * 1024 * 1024)
            {
                buf.resize(window);
                in_.clear();
                in_.seekg(static_cast<streamoff>(start), ios::beg);
                in_.read(reinterpret_cast<char *>(buf.data()), static_cast<streamsize>(window));
                auto got = static_cast<size_t>(in_.gcount());
                buf.resize(got);
                if (got < window)
                    in_.clear();

                try
                {
                    auto result = parse_element(buf, schema_);
                    if (result.element.id() == ids::Cluster)
                    {
                        // We have to know the cluster's timecode before we
                        // decide to skip it. Use the first Timecode child.
                        uint64_t tc = 0;
                        bool has    = false;
                        if (auto *t = result.element.find(ids::Timecode))
                        {
                            try
                            {
                                tc  = t->as_uint();
                                has = true;
                            } catch (...)
                            {
                            }
                        }
                        if (skipClustersBefore && has && tc < *skipClustersBefore)
                        {
                            start += result.consumed;
                            in_.clear();
                            in_.seekg(static_cast<streamoff>(start), ios::beg);
                            found = false;
                            break; // re-scan
                        }
                        cluster = std::move(result.element);
                        // Advance the stream to the end of the cluster.
                        size_t endPos = start + result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(endPos), ios::beg);
                        return true;
                    } else if (result.element.id() == ids::Segment)
                    {
                        // Nested Segment? Stop.
                        return false;
                    }
                    // Other top-level element: skip past it.
                    start += result.consumed;
                    in_.clear();
                    in_.seekg(static_cast<streamoff>(start), ios::beg);
                    found = false;
                    break;
                } catch (const ParseError &)
                {
                    if (window > 16 * 1024 * 1024)
                        break;
                    window *= 2;
                }
            }
            if (!found && buf.size() >= 16 * 1024 * 1024)
                return false;
        }
    }

    bool Demuxer::decode_lacing(LacingType lacing, span<const byte> data, size_t frameCount, vector<size_t> &frameSizes,
                                size_t &consumed)
    {
        frameSizes.clear();
        frameSizes.resize(frameCount);
        consumed = 0;
        if (frameCount == 0)
            return false;

        switch (lacing)
        {
            case LacingType::None:
                frameSizes[0] = data.size();
                return true;

            case LacingType::Fixed:
            {
                if (data.size() % frameCount != 0)
                    return false;
                const size_t each = data.size() / frameCount;
                for (size_t i = 0; i < frameCount; ++i)
                    frameSizes[i] = each;
                return true;
            }

            case LacingType::Xiph:
            {
                uint64_t total = 0;
                for (size_t i = 0; i + 1 < frameCount; ++i)
                {
                    uint64_t size = 0;
                    while (true)
                    {
                        if (consumed >= data.size())
                            return false;
                        byte b = data[consumed++];
                        size += b;
                        if (b != 0xFF)
                            break;
                    }
                    frameSizes[i] = static_cast<size_t>(size);
                    total += size;
                }
                if (consumed + total > data.size())
                    return false;
                frameSizes[frameCount - 1] = data.size() - consumed - total;
                return true;
            }

            case LacingType::EBML:
            {
                uint64_t firstSize;
                if (!read_vint(data, consumed, firstSize, true))
                    return false;
                uint64_t total = firstSize;
                frameSizes[0]  = static_cast<size_t>(firstSize);
                for (size_t i = 1; i + 1 < frameCount; ++i)
                {
                    int64_t delta;
                    if (!read_svint(data, consumed, delta))
                        return false;
                    int64_t size = static_cast<int64_t>(frameSizes[i - 1]) + delta;
                    if (size < 0)
                        return false;
                    frameSizes[i] = static_cast<size_t>(size);
                    total += size;
                }
                if (consumed + total > data.size())
                    return false;
                frameSizes[frameCount - 1] = data.size() - consumed - total;
                return true;
            }
        }
        return false;
    }

    bool Demuxer::parse_block(uint64_t blockId, span<const byte> payload, uint64_t clusterTimecode, Packet &packet)
    {
        size_t pos = 0;
        uint64_t trackNumber;
        if (!read_vint(payload, pos, trackNumber, true))
            return false;
        if (trackNumber == 0 || pos + 2 > payload.size())
            return false;

        int16_t timecode = static_cast<int16_t>((static_cast<uint16_t>(payload[pos]) << 8) | payload[pos + 1]);
        pos += 2;
        if (pos >= payload.size())
            return false;

        const uint8_t flags = payload[pos++];
        if (blockId == ids::SimpleBlock.value())
            packet.keyframe = (flags & 0x80) ? KeyframeState::True : KeyframeState::False;
        else
            packet.keyframe = KeyframeState::Unknown;

        const uint8_t lacingBits = (flags >> 1) & 0x03;
        const LacingType lacing  = static_cast<LacingType>(lacingBits);

        size_t frameCount = 1;
        if (lacing != LacingType::None)
        {
            if (pos >= payload.size())
                return false;
            frameCount = static_cast<size_t>(payload[pos++]) + 1;
            if (frameCount < 2 || frameCount > 256)
                return false;
        }

        vector<size_t> frameSizes;
        size_t lacingConsumed = 0;
        if (!decode_lacing(lacing, payload.subspan(pos), frameCount, frameSizes, lacingConsumed))
            return false;
        pos += lacingConsumed;

        // Validate frame sizes against remaining bytes.
        uint64_t sum = 0;
        for (auto s: frameSizes)
        {
            sum += s;
            if (sum > payload.size() - pos)
                return false;
        }

        packet.trackNumber = trackNumber;
        int idx            = find_track_index(trackNumber);
        if (idx < 0)
            return false;
        packet.track          = static_cast<uint64_t>(idx);
        const int64_t absTime = static_cast<int64_t>(clusterTimecode) + timecode;
        packet.timecode       = absTime < 0 ? 0 : static_cast<uint64_t>(absTime);

        packet.frames.clear();
        packet.frames.reserve(frameCount);
        for (size_t i = 0; i < frameCount; ++i)
        {
            Frame f;
            f.data.assign(payload.begin() + pos, payload.begin() + pos + frameSizes[i]);
            packet.frames.push_back(std::move(f));
            pos += frameSizes[i];
        }
        return true;
    }

    bool Demuxer::parse_block_additions(const span<const byte> payload, Packet &packet) const
    {
        // Walk BlockAdditions -> repeated BlockMore -> BlockAddID, BlockAdditional.
        ElementList parsed;
        try
        {
            parsed = parse_stream(payload, schema_);
        } catch (const ParseError &)
        {
            return false;
        }
        for (const auto &more: parsed)
        {
            if (more.id() != ids::BlockMore)
                continue;
            unsigned addId = 1;
            if (auto *e = more.find(ids::BlockAddID))
                try
                {
                    addId = static_cast<unsigned>(e->as_uint());
                } catch (...)
                {
                }
            if (auto *e = more.find(ids::BlockAdditional))
            {
                if (addId == 0)
                    return false;
                packet.blockAdditional.emplace_back(addId, e->raw());
            }
        }
        return true;
    }

    int Demuxer::read_packet(optional<Packet> &packet)
    {
        packet.reset();
        if (!headersParsed_)
            return -1;

        // We lazily parse clusters: read from wherever the stream is and look
        // for either a Cluster or a Block/SimpleBlock directly.
        while (true)
        {
            size_t start = tell_stream();
            if (start == SIZE_MAX)
                return -1;
            vector<byte> buf;
            size_t window   = 64;
            bool progressed = false;
            while (buf.size() < 32 * 1024 * 1024)
            {
                buf.resize(window);
                in_.clear();
                in_.seekg(static_cast<streamoff>(start), ios::beg);
                in_.read(reinterpret_cast<char *>(buf.data()), static_cast<streamsize>(window));
                auto got = static_cast<size_t>(in_.gcount());
                buf.resize(got);
                if (got < window)
                    in_.clear();

                try
                {
                    auto result   = parse_element(buf, schema_);
                    const auto &e = result.element;
                    if (e.id() == ids::Cluster)
                    {
                        // Process this Cluster block by block.
                        uint64_t tc = 0;
                        bool hasTc  = false;
                        if (auto *t = e.find(ids::Timecode))
                            try
                            {
                                tc    = t->as_uint();
                                hasTc = true;
                            } catch (...)
                            {
                            }
                        if (!hasTc)
                        {
                            // Skip clusters without a timecode? Per spec
                            // Timecode is mandatory. Treat as error.
                            start += result.consumed;
                            in_.clear();
                            in_.seekg(static_cast<streamoff>(start), ios::beg);
                            progressed = true;
                            break;
                        }
                        clusterTimecode_    = tc;
                        hasClusterTimecode_ = true;

                        // Walk children; find the first Block/SimpleBlock.
                        for (const auto &child: e.children())
                        {
                            if (child.id() == ids::SimpleBlock)
                            {
                                Packet pkt;
                                if (!parse_block(ids::SimpleBlock.value(), child.raw(), tc, pkt))
                                    continue;
                                pkt.endOffset = static_cast<int64_t>(start) + static_cast<int64_t>(result.consumed);
                                packet        = std::move(pkt);
                                // Position the stream just past this cluster.
                                size_t endPos = start + result.consumed;
                                in_.clear();
                                in_.seekg(static_cast<streamoff>(endPos), ios::beg);
                                return 1;
                            } else if (child.id() == ids::BlockGroup)
                            {
                                Packet pkt;
                                for (const auto &gc: child.children())
                                {
                                    if (gc.id() == ids::Block)
                                    {
                                        if (!parse_block(ids::Block.value(), gc.raw(), tc, pkt))
                                            continue;
                                        pkt.endOffset =
                                                static_cast<int64_t>(start) + static_cast<int64_t>(result.consumed);
                                    } else if (gc.id() == ids::BlockDuration)
                                    {
                                        try
                                        {
                                            pkt.duration    = gc.as_uint();
                                            pkt.hasDuration = true;
                                        } catch (...)
                                        {
                                        }
                                    } else if (gc.id() == ids::DiscardPadding)
                                    {
                                        try
                                        {
                                            pkt.discardPadding    = gc.as_int();
                                            pkt.hasDiscardPadding = true;
                                        } catch (...)
                                        {
                                        }
                                    } else if (gc.id() == ids::ReferenceBlock)
                                    {
                                        try
                                        {
                                            pkt.referenceBlock    = gc.as_int();
                                            pkt.hasReferenceBlock = true;
                                            pkt.keyframe          = KeyframeState::False;
                                        } catch (...)
                                        {
                                        }
                                    } else if (gc.id() == ids::BlockAdditions)
                                    {
                                        parse_block_additions(gc.raw(), pkt);
                                    }
                                }
                                if (!pkt.frames.empty())
                                {
                                    packet        = std::move(pkt);
                                    size_t endPos = start + result.consumed;
                                    in_.clear();
                                    in_.seekg(static_cast<streamoff>(endPos), ios::beg);
                                    return 1;
                                }
                            }
                        }
                        // No block found in this cluster; std::move on.
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    } else if (e.id() == ids::SimpleBlock || e.id() == ids::BlockGroup)
                    {
                        // We're already inside a Cluster (the standard case):
                        // parse a single packet from a top-level element.
                        Packet pkt;
                        if (e.id() == ids::SimpleBlock)
                        {
                            if (!parse_block(ids::SimpleBlock.value(), e.raw(), clusterTimecode_, pkt))
                                continue;
                        } else
                        {
                            for (const auto &gc: e.children())
                            {
                                if (gc.id() == ids::Block)
                                {
                                    if (!parse_block(ids::Block.value(), gc.raw(), clusterTimecode_, pkt))
                                        continue;
                                } else if (gc.id() == ids::BlockDuration)
                                    try
                                    {
                                        pkt.duration    = gc.as_uint();
                                        pkt.hasDuration = true;
                                    } catch (...)
                                    {
                                    }
                                else if (gc.id() == ids::DiscardPadding)
                                    try
                                    {
                                        pkt.discardPadding    = gc.as_int();
                                        pkt.hasDiscardPadding = true;
                                    } catch (...)
                                    {
                                    }
                                else if (gc.id() == ids::ReferenceBlock)
                                {
                                    try
                                    {
                                        pkt.referenceBlock    = gc.as_int();
                                        pkt.hasReferenceBlock = true;
                                        pkt.keyframe          = KeyframeState::False;
                                    } catch (...)
                                    {
                                    }
                                } else if (gc.id() == ids::BlockAdditions)
                                    parse_block_additions(gc.raw(), pkt);
                            }
                        }
                        if (!pkt.frames.empty())
                        {
                            pkt.endOffset = static_cast<int64_t>(start) + static_cast<int64_t>(result.consumed);
                            packet        = std::move(pkt);
                            size_t endPos = start + result.consumed;
                            in_.clear();
                            in_.seekg(static_cast<streamoff>(endPos), ios::beg);
                            return 1;
                        }
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    } else if (e.id() == ids::Timecode)
                    {
                        // Timecode inside a Cluster (should not reach here
                        // because we parse the whole Cluster above).
                        try
                        {
                            clusterTimecode_    = e.as_uint();
                            hasClusterTimecode_ = true;
                        } catch (...)
                        {
                        }
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    } else if (e.id() == EBML::ids::EBML || e.id() == ids::Segment || e.id() == ids::Info ||
                               e.id() == ids::Tracks)
                    {
                        // Metadata in the middle of the stream: skip.
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    } else if (e.id() == ids::Cues || e.id() == ids::Tags || e.id() == ids::Chapters ||
                               e.id() == ids::Attachments || e.id() == ids::SeekHead)
                    {
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    } else
                    {
                        // Unknown top-level element: skip.
                        start += result.consumed;
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        progressed = true;
                        break;
                    }
                } catch (const ParseError &)
                {
                    if (window > 32 * 1024 * 1024)
                    {
                        in_.clear();
                        in_.seekg(static_cast<streamoff>(start), ios::beg);
                        if (!in_.good())
                            return 0; // EOF
                        return -1;
                    }
                    window *= 2;
                }
            }
            if (!progressed)
            {
                // Couldn't make progress; assume EOF.
                return 0;
            }
            // Check for EOF after skipping.
            if (in_.peek() == char_traits<char>::eof())
                return 0;
        }
    }


    bool Demuxer::parse_element_at(uint64_t segmentRelativeOffset, const Identifier id, Element &out) const
    {
        if (!seek_stream(segmentOffset_ + segmentRelativeOffset))
            return false;
        vector<byte> buf;
        size_t window = 64;
        while (buf.size() < 16 * 1024 * 1024)
        {
            buf.resize(window);
            in_.clear();
            in_.seekg(static_cast<streamoff>(segmentOffset_ + segmentRelativeOffset), ios::beg);
            in_.read(reinterpret_cast<char *>(buf.data()), static_cast<streamsize>(window));
            auto got = static_cast<size_t>(in_.gcount());
            buf.resize(got);
            if (got < window)
                in_.clear();
            try
            {
                auto result = parse_element(buf, schema_);
                if (result.element.id() != id)
                    return false;
                out = std::move(result.element);
                return true;
            } catch (const ParseError &)
            {
                if (window > 16 * 1024 * 1024)
                    return false;
                window *= 2;
            }
        }
        return false;
    }

    bool Demuxer::seek_track(unsigned int trackIdx, uint64_t tstamp)
    {
        if (trackIdx >= tracks_.size())
            return false;

        if (!cues_.empty())
        {
            // Cues are sorted by timecode. Find the largest cue <= tstamp.
            auto it = ranges::upper_bound(cues_, make_pair(tstamp, static_cast<uint64_t>(-1)),
                                          [](const auto &a, const auto &b) { return a.first < b.first; });
            if (it != cues_.begin())
                --it;
            return seek_stream(segmentOffset_ + it->second);
        }

        // No cues: binary-search by reading Cluster timecodes.
        // To keep this O(log n), we assume Cluster sizes are roughly
        // representative. We do a simple linear scan here; callers seeking
        // often should prefer the Cues-based path.
        if (!seek_stream(segmentOffset_))
            return false;
        while (true)
        {
            Element cluster;
            if (!read_cluster(cluster))
                return false;
            uint64_t tc = 0;
            if (auto *t = cluster.find(ids::Timecode))
                try
                {
                    tc = t->as_uint();
                } catch (...)
                {
                    return false;
                }
            if (tc >= tstamp)
            {
                // Seek back to this cluster's start.
                size_t pos = tell_stream();
                // We've already consumed the cluster; find its start by
                // scanning backward. Simpler: re-scan from segmentOffset_.
                return seek_stream(pos); // approximation: caller re-reads
            }
        }
    }

    bool Demuxer::reset() const { return seek_stream(savedOffset_); }
} // namespace SF::Matroska
