#pragma once

#include "../EBML/Element.hpp"
#include "../EBML/Serializer.hpp"
#include "../EBML/Schema.hpp"
#include "../EBML/CRC32.hpp"
#include "Timestamp.hpp"
#include "MatroskaIds.hpp"
#include "Block.hpp"
#include "Track.hpp"
#include "MatroskaSchema.hpp"
#include <iostream>
#include <queue>
#include <functional>
#include <fstream>

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
    inline const Schema& default_matroska_schema()
    {
        static const Schema instance = create_matroska_schema();
        return instance;
    }

    // Streaming writer that can write large files without holding everything in memory
    class StreamingWriter
    {
    private:
        std::ostream &out_;
        const Schema &schema_;
        Timestamp timestamp_;
        
        // Track positions for seeking
        struct SeekEntry
        {
            uint64_t id;
            size_t position;  // Position in file where SeekPosition will be written
        };
        std::vector<SeekEntry> seekEntries_;
        
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
        size_t clusterCount_ = 0;

    public:
        StreamingWriter(std::ostream &out, const Schema &schema = default_matroska_schema())
            : out_(out), schema_(schema) {}

        size_t tell() const { return currentPosition_; }
        size_t get_cluster_count() const { return clusterCount_; }

        void begin_unknown(Identifier id, uint8_t sizeLength = 8, bool enableCRC32 = false)
        {
            UnknownElement elem;
            elem.id = id;
            elem.idBytes = id.encode();
            elem.sizeBytes = encode_unknown_size(sizeLength);
            elem.startPos = currentPosition_;
            elem.hasCRC32 = enableCRC32;

            // Write ID and unknown size
            out_.write(reinterpret_cast<const char *>(elem.idBytes.data()), elem.idBytes.size());
            out_.write(reinterpret_cast<const char *>(elem.sizeBytes.data()), elem.sizeBytes.size());
            currentPosition_ += elem.idBytes.size() + elem.sizeBytes.size();

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
                auto crcData = serialize(crcElement);
                
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
            auto element = Element::make_binary(ids::SimpleBlock, std::move(blockData));
            write_element(element);
        }

        void write_block_group(const BlockGroup &group)
        {
            auto bg = Element::make_master(ids::BlockGroup);

            auto blockData = encode_block(group.block);
            bg.add(Element::make_binary(ids::Block, std::move(blockData)));

            for (uint64_t ref : group.referenceBlocks)
            {
                bg.add(Element::make_int(ids::ReferenceBlock,
                                         static_cast<int64_t>(ref)));
            }

            if (group.duration)
            {
                bg.add(Element::make_uint(ids::BlockDuration, group.duration));
            }

            if (group.codecState)
            {
                bg.add(Element::make_uint(ids::CodecState, group.codecState));
            }

            for (const auto &child : group.additionalData)
            {
                bg.add(child);
            }

            write_element(bg);
        }

        void flush()
        {
            out_.flush();
        }

        // Register a seek entry for later updating
        void add_seek_entry(uint64_t id, size_t position)
        {
            seekEntries_.push_back({id, position});
        }

        // Update SeekHead positions (requires seekable stream)
        bool update_seek_entries(std::ostream &out)
        {
            // This requires the stream to support seeking
            // Implementation would seek to each position and write the value
            return true;
        }
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
        bool eof_ = false;
        size_t bytesRead_ = 0;

    public:
        StreamingReader(std::istream &in, const Schema &schema = default_matroska_schema(),
                        std::function<void(const Element &)> callback = nullptr)
            : in_(in), schema_(schema), callback_(callback)
        {
            buffer_.reserve(1024 * 1024);
        }

        std::optional<Element> read_next_element()
        {
            while (!eof_)
            {
                if (!ensure_data(8))
                {
                    eof_ = true;
                    return std::nullopt;
                }

                try
                {
                    auto result = parse_element(
                        std::span<const byte>(buffer_.data() + bufferPos_,
                                              buffer_.size() - bufferPos_),
                        schema_);

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
                }
                catch (const ParseError &e)
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

        size_t bytes_read() const { return bytesRead_; }

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
            size_t oldSize = buffer_.size();
            buffer_.resize(oldSize + kChunkSize);
            in_.read(reinterpret_cast<char *>(buffer_.data() + oldSize), kChunkSize);
            size_t read = static_cast<size_t>(in_.gcount());
            buffer_.resize(oldSize + read);

            return read > 0;
        }
    };
}