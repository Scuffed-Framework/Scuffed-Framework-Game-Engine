#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../EBML/CRC32.hpp"
#include "../EBML/Element.hpp"
#include "../EBML/Schema.hpp"
#include "../EBML/Serializer.hpp"
#include "Block.hpp"
#include "MatroskaIds.hpp"
#include "MatroskaSchema.hpp"
#include "Timestamp.hpp"
#include "Track.hpp"

namespace SF::Matroska
{
    using namespace SF::EBML;

    // A Schema built by create_matroska_schema() as a *default argument* is a
    // temporary that only lives to the end of the constructor call it's
    // passed into, it is NOT lifetime-extended by binding to a reference
    // *member* (that lifetime-extension rule only applies to reference
    // *variables*, not members initialized via a mem-initializer). Any class
    // storing `const Schema&` and defaulting it to create_matroska_schema()
    // ends up with a dangling reference the moment construction finishes,
    // corrupting every later parse in ways that vary by build/run (silently
    // wrong results, spurious errors, or a crash) since it's reading freed
    // memory. This function-local static has program lifetime, so binding a
    // reference to it, including as a default argument; is always safe.
    inline const Schema &default_matroska_schema()
    {
        static const Schema instance = create_matroska_schema();
        return instance;
    }

    // Streaming writer that can write large files without holding everything in memory
    class StreamingWriter
    {
    private:
        std::ostream &out_;
        Timestamp timestamp_;

        // Track positions for seeking
        struct SeekEntry
        {
            uint64_t id;     // id of the element this entry points at
            size_t position; // absolute offset in the stream of the 8-byte SeekPosition payload
        };
        std::vector<SeekEntry> seekEntries_;

        // Absolute offset of the first occurrence of every element id written through
        // begin_unknown / write_element. Used to resolve seek entries.
        std::unordered_map<uint64_t, size_t> elementPositions_;
        // Absolute offset of the first byte of the Segment's data. SeekPosition values
        // are relative to this.
        std::optional<size_t> segmentDataStart_;

        struct UnknownElement
        {
            size_t startPos = 0;
            Identifier id;
            std::vector<byte> idBytes;
            std::vector<byte> sizeBytes;
            bool hasCRC32 = false;
            std::vector<byte> crcBuffer;
        };
        std::vector<UnknownElement> unknownStack_;

        // File position tracking
        size_t currentPosition_ = 0;
        size_t clusterCount_    = 0;

    public:
        explicit StreamingWriter(std::ostream &out) :
            out_(out)
        {
            // Start from wherever the stream already is so recorded offsets are absolute.
            const std::streampos p = out_.tellp();
            if (p != std::streampos(-1))
                currentPosition_ = static_cast<size_t>(static_cast<std::streamoff>(p));
        }

        size_t tell() const { return currentPosition_; }
        size_t get_cluster_count() const { return clusterCount_; }

        void begin_unknown(Identifier id, uint8_t sizeLength = 8, bool enableCRC32 = false)
        {
            UnknownElement elem;
            elem.id        = id;
            elem.idBytes   = id.encode();
            elem.sizeBytes = encode_unknown_size(sizeLength);
            elem.startPos  = currentPosition_;
            elem.hasCRC32  = enableCRC32;

            elementPositions_.try_emplace(id.value(), currentPosition_);

            // Write ID and unknown size
            out_.write(reinterpret_cast<const char *>(elem.idBytes.data()), elem.idBytes.size());
            out_.write(reinterpret_cast<const char *>(elem.sizeBytes.data()), elem.sizeBytes.size());
            currentPosition_ += elem.idBytes.size() + elem.sizeBytes.size();

            if (id.value() == ids::Segment.value() && !segmentDataStart_)
                segmentDataStart_ = currentPosition_; // data starts right after the Segment header

            unknownStack_.push_back(std::move(elem));
        }

        void end_unknown()
        {
            if (unknownStack_.empty())
                throw std::runtime_error("No unknown-size element to close");

            auto &elem = unknownStack_.back();

            // If this element has CRC32, write it at the beginning
            if (elem.hasCRC32 && !elem.crcBuffer.empty())
            {
                // Calculate CRC32 of all children
                uint32_t crc = crc32(elem.crcBuffer);

                // Build CRC32 element
                std::vector<byte> crcBytes = {
                        static_cast<byte>(crc & 0xFF),
                        static_cast<byte>((crc >> 8) & 0xFF),
                        static_cast<byte>((crc >> 16) & 0xFF),
                        static_cast<byte>((crc >> 24) & 0xFF),
                };
                auto crcElement = Element::make_binary(::SF::EBML::ids::CRC32, std::move(crcBytes));
                auto crcData    = serialize(crcElement);

                // Seek back to after the ID and size
                // But we need to insert CRC32 at the beginning, which requires shifting
                // This is complex - for now, we'll write it at the end
                out_.write(reinterpret_cast<const char *>(crcData.data()), crcData.size());
                currentPosition_ += crcData.size();

                elem.crcBuffer.clear();
            }

            unknownStack_.pop_back();
        }

        void write_element(const Element &element)
        {
            const size_t start = currentPosition_;
            elementPositions_.try_emplace(element.id().value(), start);
            if (element.id().value() == ids::Segment.value() && !segmentDataStart_)
                segmentDataStart_ = start + payload_offset(element);

            auto bytes = serialize(element);
            out_.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            currentPosition_ += bytes.size();

            // Accumulate for CRC32 if needed
            if (!unknownStack_.empty())
            {
                auto &elem = unknownStack_.back();
                if (elem.hasCRC32)
                {
                    elem.crcBuffer.insert(elem.crcBuffer.end(), bytes.begin(), bytes.end());
                }
            }
        }

        void write_simple_block(const SimpleBlock &block)
        {
            auto blockData = encode_simple_block(block);
            auto element   = Element::make_binary(ids::SimpleBlock, std::move(blockData));
            write_element(element);
        }

        void write_block_group(const BlockGroup &group)
        {
            auto bg = Element::make_master(ids::BlockGroup);

            auto blockData = encode_block(group.block);
            bg.add(Element::make_binary(ids::Block, std::move(blockData)));

            for (uint64_t ref: group.referenceBlocks)
            {
                bg.add(Element::make_int(ids::ReferenceBlock, static_cast<int64_t>(ref)));
            }

            if (group.duration)
            {
                bg.add(Element::make_uint(ids::BlockDuration, group.duration));
            }

            if (group.codecState)
            {
                bg.add(Element::make_uint(ids::CodecState, group.codecState));
            }

            for (const auto &child: group.additionalData)
            {
                bg.add(child);
            }

            write_element(bg);
        }

        void flush() { out_.flush(); }

        // Overrides the Segment data start used to make SeekPosition values relative.
        // Normally detected automatically when a Segment is written.
        void set_segment_data_start(size_t position) { segmentDataStart_ = position; }

        // Register a seek entry for later updating. `position` is the absolute stream offset
        // of an 8-byte big-endian SeekPosition payload that was written as a placeholder.
        void add_seek_entry(uint64_t id, size_t position) { seekEntries_.push_back({id, position}); }

        /**
         * @brief Writes a SeekHead with one Seek entry per target id, each with a fixed 8-byte
         *        SeekPosition placeholder, and registers those placeholders so that
         *        update_seek_entries() can fill them in once the targets have been written.
         *        Call it inside the Segment, before the elements it points at.
         */
        void write_seek_head(const std::vector<Identifier> &targets)
        {
            const size_t seekPosHeaderLen = ids::SeekPosition.encode().size() + encode_size(8).size();

            std::vector<Element> entries;
            entries.reserve(targets.size());
            size_t bodyLen = 0;
            for (const auto &target: targets)
            {
                auto seek = Element::make_master(ids::Seek);
                seek.add(Element::make_binary(ids::SeekID, target.encode()));
                // Fixed 8 bytes so the SeekHead never changes size when patched.
                seek.add(Element::make_binary(ids::SeekPosition, std::vector<byte>(8, 0)));
                bodyLen += serialized_size(seek);
                entries.push_back(std::move(seek));
            }

            auto head = Element::make_master(ids::SeekHead);

            // Entries start after the SeekHead's own id + size field.
            size_t entryStart = currentPosition_ + head.id().encode().size() + encode_size(bodyLen).size();
            for (size_t i = 0; i < entries.size(); ++i)
            {
                SerializationContext ctx;
                ctx.currentPosition   = entryStart;
                const size_t entrySz  = serialize(entries[i], &ctx).size(); // only to learn where SeekPosition lands
                const size_t posStart = ctx.elementPositions.at(ids::SeekPosition.value());
                add_seek_entry(targets[i].value(), posStart + seekPosHeaderLen);
                entryStart += entrySz;
            }

            for (auto &e: entries)
                head.add(std::move(e));
            write_element(head);
        }

        /**
         * @brief Fills every registered SeekPosition placeholder with the position of its target
         *        element, relative to the Segment data start, as an 8-byte big-endian uint.
         *        Writes through `target`, which must be seekable and refer to the same output
         *        (normally the writer's own stream; see the no-argument overload).
         * @return false if the stream isn't seekable, the Segment start is unknown, or any entry's
         *         target element was never written (that entry is left as its placeholder).
         */
        bool update_seek_entries(std::ostream &target)
        {
            if (!segmentDataStart_)
                return false;

            const std::streampos resume = target.tellp();
            if (resume == std::streampos(-1))
                return false; // not seekable (pipe, socket, ...)

            bool allResolved = true;
            for (const auto &entry: seekEntries_)
            {
                const auto it = elementPositions_.find(entry.id);
                if (it == elementPositions_.end() || it->second < *segmentDataStart_)
                {
                    allResolved = false;
                    continue;
                }

                uint64_t value = it->second - *segmentDataStart_;
                std::array<byte, 8> bytes{};
                for (int i = 7; i >= 0; --i)
                {
                    bytes[static_cast<size_t>(i)] = static_cast<byte>(value & 0xFF);
                    value >>= 8;
                }

                target.seekp(static_cast<std::streamoff>(entry.position));
                target.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }

            target.seekp(resume); // leave the stream where the caller was writing
            return allResolved && target.good();
        }

        // Convenience: patch through the writer's own stream.
        bool update_seek_entries() { return update_seek_entries(out_); }
    };

    // Streaming reader
    class StreamingReader
    {
    private:
        std::istream &in_;
        const Schema &schema_;
        std::function<void(const Element &)> callback_;
        std::vector<byte> buffer_;
        size_t bufferPos_ = 0;
        bool eof_         = false;
        size_t bytesRead_ = 0;

    public:
        StreamingReader(std::istream &in, const Schema &schema = default_matroska_schema(),
                        std::function<void(const Element &)> callback = nullptr) :
            in_(in), schema_(schema), callback_(std::move(callback))
        {
            buffer_.reserve(1024 * 1024);
        }

        std::optional<Element> read_next_element()
        {
            while (!eof_)
            {
                // Any remaining byte is worth a parse attempt: a valid element can be shorter
                // than 8 bytes (e.g. a small Void at the end of the file). If it's truncated,
                // the ParseError path below reads more data or reports it at EOF.
                if (!ensure_data(1))
                {
                    eof_ = true;
                    return std::nullopt;
                }

                try
                {
                    auto result = parse_element(
                            std::span<const byte>(buffer_.data() + bufferPos_, buffer_.size() - bufferPos_), schema_);

                    bufferPos_ += result.consumed;
                    bytesRead_ += result.consumed;

                    if (bufferPos_ > buffer_.size() / 2)
                    {
                        buffer_.erase(buffer_.begin(), buffer_.begin() + bufferPos_);
                        bufferPos_ = 0;
                    }

                    if (callback_)
                    {
                        callback_(result.element);
                    }

                    return std::move(result.element);
                } catch (const ParseError &)
                {
                    if (!read_more_data())
                    {
                        eof_ = true;
                        throw;
                    }
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] size_t bytes_read() const { return bytesRead_; }

    private:
        bool ensure_data(size_t minBytes)
        {
            while (buffer_.size() - bufferPos_ < minBytes)
            {
                if (!read_more_data())
                    return false;
            }
            return true;
        }

        bool read_more_data()
        {
            if (bufferPos_ > 0)
            {
                buffer_.erase(buffer_.begin(), buffer_.begin() + bufferPos_);
                bufferPos_ = 0;
            }

            static constexpr size_t kChunkSize = 1024 * 1024;
            size_t oldSize                     = buffer_.size();
            buffer_.resize(oldSize + kChunkSize);
            in_.read(reinterpret_cast<char *>(buffer_.data() + oldSize), kChunkSize);
            auto read = static_cast<size_t>(in_.gcount());
            buffer_.resize(oldSize + read);

            return read > 0;
        }
    };
} // namespace SF::Matroska
