#pragma once

#include <istream>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "../EBML/Element.hpp"
#include "../EBML/Schema.hpp"
#include "Block.hpp"
#include "MatroskaIds.hpp"
#include "MatroskaSchema.hpp"
#include "Streaming.hpp"
#include "Track.hpp"

namespace SF::Matroska
{
    using namespace std;
    using namespace SF::EBML;

    enum class KeyframeState : uint8_t
    {
        Unknown,
        True,
        False,
    };

    struct Packet
    {
        uint64_t track         = 0; // track index (0-based, matches Tracks order)
        uint64_t trackNumber   = 0; // raw Matroska TrackNumber
        uint64_t timecode      = 0; // absolute timestamp in timecode-scale units
        uint64_t duration      = 0; // BlockDuration * timecodeScale (0 if absent)
        bool hasDuration       = false;
        int64_t discardPadding = 0;
        bool hasDiscardPadding = false;
        int64_t referenceBlock = 0;
        bool hasReferenceBlock = false;
        KeyframeState keyframe = KeyframeState::Unknown;
        int64_t endOffset      = -1; // stream offset just past this Block/Group
        vector<Frame> frames;        // laced frames, already de-laced
        vector<pair<unsigned, vector<byte>>> blockAdditional;
    };

    class Demuxer
    {
        istream &in_;
        const Schema &schema_;
        size_t streamStart_       = 0;
        size_t segmentOffset_     = 0;
        uint64_t timecodeScale_   = 1000000;
        bool hasTimecodeScale_    = false;
        uint64_t clusterTimecode_ = 0;
        bool hasClusterTimecode_  = false;

        // Tracks, indexed by their position in the Tracks element.
        vector<Track> tracks_;
        unordered_map<uint64_t, unsigned> trackNumberToIndex_;

        // Cues for seeking.
        struct CuePosition
        {
            uint64_t clusterPosition = 0;
        };
        vector<pair<uint64_t, uint64_t>> cues_; // (timecode, clusterPosition)
        uint64_t cuesPosition_ = 0;
        bool hasCues_          = false;

        // Parsed headers.
        vector<byte> ebmlHeaderBytes_;
        bool headersParsed_ = false;

        // SeekHead entries: (targetId, segment-relative offset).
        unordered_map<uint64_t, uint64_t> seekHeadEntries_;
        bool hasSeekHead_ = false;

        // Info/Tracks/Tags/Attachments positions.
        optional<uint64_t> infoPosition_;
        optional<uint64_t> tracksPosition_;
        optional<uint64_t> tagsPosition_;
        optional<uint64_t> chaptersPosition_;
        optional<uint64_t> attachmentsPosition_;

        // Save point for read_reset.
        size_t savedOffset_ = 0;

    public:
        Demuxer(istream &in, const Schema &schema = default_matroska_schema()) : in_(in), schema_(schema) {}

        // Parse the EBML header and the first Segment's initial children.
        // Returns false if this is not a Matroska/WebM stream.
        bool initialize(bool seekToFirstCluster = true);

        // Track accessors (0-based indices match Tracks order).
        size_t track_count() const { return tracks_.size(); }
        const Track &track(unsigned int index) const
        {
            if (index >= tracks_.size())
                throw out_of_range("track index out of range");
            return tracks_[index];
        }

        int find_track_index(uint64_t trackNumber) const;

        uint64_t timecode_scale() const { return timecodeScale_; }

        // Optional duration parsed from Info, if present.
        optional<uint64_t> duration() const;

        // Read the next Block/SimpleBlock. Returns:
        //   1  - packet produced (*packet populated)
        //   0  - EOF
        //  -1  - error
        // `packet` must be empty before the call (we don't free caller memory).
        int read_packet(optional<Packet> &packet);

        // Save the current position; subsequent read_packet errors can resume
        // by calling reset().
        void save() { savedOffset_ = position(); }
        bool reset() const;

        // Seek to the cluster containing tstamp (in timecode-scale units) on track.
        // Falls back to a binary search across clusters if no cues exist.
        bool seek_track(unsigned int track, uint64_t tstamp);

        // Byte seek to an absolute offset in the stream.
        bool seek_to(size_t offset) const;

    private:
        size_t position() const;
        size_t tell_stream() const;
        bool seek_stream(size_t offset) const;

        bool parse_ebml_header();
        bool parse_segment_header();
        bool parse_segment_metadata();
        bool parse_info(const Element &info);
        bool parse_tracks(Element &tracks);
        bool parse_seek_head(Element &seekHead);
        bool parse_cues(Element &cues);

        // Locate an element by its SeekHead entry and parse it.
        bool parse_element_at(uint64_t segmentRelativeOffset, Identifier id, Element &out) const;

        // Read one Cluster from the current position. Returns the cluster
        // Element; if `skipClustersBefore` is set, clusters whose timecode is
        // below it are skipped (used for seeking).
        bool read_cluster(Element &cluster, optional<uint64_t> skipClustersBefore = nullopt) const;

        // Parse a Block/SimpleBlock payload into a Packet.
        bool parse_block(uint64_t blockId, span<const byte> payload, uint64_t clusterTimecode, Packet &packet);

        // Parse BlockAdditions inside a BlockGroup.
        bool parse_block_additions(span<const byte> payload, Packet &packet) const;

        // Decode lacing. On entry, `data` begins just past the flags byte.
        // `consumed` returns how many bytes of `data` were consumed by lacing headers.
        bool decode_lacing(LacingType lacing, span<const byte> data, size_t frameCount, vector<size_t> &frameSizes,
                           size_t &consumed);

        // Read a VINT from a byte span, advancing `pos`. Returns false on
        // truncation or invalid VINT.
        bool read_vint(span<const byte> data, size_t &pos, uint64_t &value, bool maskFirstBit);
        bool read_svint(span<const byte> data, size_t &pos, int64_t &value);
    };
} // namespace SF::Matroska
