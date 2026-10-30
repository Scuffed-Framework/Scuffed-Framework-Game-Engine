#pragma once

#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include "../EBML/Element.hpp"
#include "../EBML/VINT.hpp"

using namespace SF::EBML;
namespace SF::Matroska
{
    using namespace std;
    // Lacing types as defined in Matroska spec
    enum class LacingType : uint8_t
    {
        None  = 0x00,
        Xiph  = 0x01,
        Fixed = 0x02,
        EBML  = 0x03
    };

    struct Frame
    {
        vector<EBML::byte> data; // Use SF::EBML::byte
        int64_t duration = 0;    // For lacing, in timecode scale units
    };

    struct SimpleBlock
    {
        uint64_t trackNumber{};
        int16_t timecode{}; // Relative to cluster timecode
        bool keyframe     = true;
        bool invisible    = false;
        bool discardable  = false; // Only for SimpleBlock
        LacingType lacing = LacingType::None;
        vector<Frame> frames;
    };

    struct Block
    {
        uint64_t trackNumber;
        int16_t timecode; // Relative to cluster timecode
        bool invisible    = false;
        LacingType lacing = LacingType::None;
        vector<Frame> frames;
        // Block has no keyframe/discardable flags (those are in BlockGroup)
    };

    // Encode a SimpleBlock as bytes
    inline vector<EBML::byte> encode_simple_block(const SimpleBlock &block)
    {
        vector<EBML::byte> out;

        // Track number (VINT)
        auto trackBytes = encode_size(block.trackNumber);
        out.insert(out.end(), trackBytes.begin(), trackBytes.end());

        // Timecode (signed 16-bit, big-endian)
        int16_t tc = block.timecode;
        out.push_back(static_cast<EBML::byte>((tc >> 8) & 0xFF));
        out.push_back(static_cast<EBML::byte>(tc & 0xFF));

        // Flags byte
        uint8_t flags = 0;
        if (block.keyframe)
            flags |= 0x80; // SimpleBlock: bit set = keyframe
        if (block.invisible)
            flags |= 0x08;
        if (block.lacing != LacingType::None)
        {
            flags |= (static_cast<uint8_t>(block.lacing) & 0x03) << 1;
        }
        if (block.discardable)
            flags |= 0x01;
        out.push_back(static_cast<EBML::byte>(flags));

        // Frame data
        if (block.lacing == LacingType::None)
        {
            if (block.frames.empty())
                throw runtime_error("No frames in block");
            out.insert(out.end(), block.frames[0].data.begin(), block.frames[0].data.end());
            return out;
        }

        // Lacing: number of frames - 1
        size_t frameCount = block.frames.size();
        if (frameCount < 2 || frameCount > 256)
            throw runtime_error("Lacing requires 2-256 frames");
        out.push_back(static_cast<EBML::byte>(frameCount - 1));

        // Calculate and encode frame sizes
        vector<uint64_t> sizes;
        if (block.lacing == LacingType::Fixed)
        {
            // Fixed lacing: all frames same size
            const uint64_t frameSize = block.frames[0].data.size();
            for (const auto &[data, duration]: block.frames)
            {
                if (data.size() != frameSize)
                    throw runtime_error("Fixed lacing requires all frames same size");
            }
            // No size encoding needed
        } else if (block.lacing == LacingType::Xiph)
        {
            // Xiph lacing: sizes encoded with 0xFF continuation
            for (size_t i = 0; i < frameCount - 1; ++i)
            {
                uint64_t size = block.frames[i].data.size();
                sizes.push_back(size);

                if (size < 0xFF)
                {
                    out.push_back(static_cast<EBML::byte>(size));
                } else
                {
                    uint64_t remaining = size;
                    while (remaining >= 0xFF)
                    {
                        out.push_back(0xFF);
                        remaining -= 0xFF;
                    }
                    out.push_back(static_cast<EBML::byte>(remaining));
                }
            }
        } else if (block.lacing == LacingType::EBML)
        {
            // EBML lacing: sizes encoded as VINTs
            for (size_t i = 0; i < frameCount - 1; ++i)
            {
                uint64_t size = block.frames[i].data.size();
                sizes.push_back(size);
                auto sizeBytes = encode_size(size);
                out.insert(out.end(), sizeBytes.begin(), sizeBytes.end());
            }
        }

        // Frame data
        for (const auto &frame: block.frames)
        {
            out.insert(out.end(), frame.data.begin(), frame.data.end());
        }

        return out;
    }

    // Decode a SimpleBlock from bytes
    inline SimpleBlock decode_simple_block(span<const EBML::byte> data, bool isSimpleBlock = true)
    {
        if (data.empty())
            throw runtime_error("Empty SimpleBlock data");

        size_t pos = 0;

        // Decode track number
        auto trackResult = decode_size(data.subspan(pos));
        if (!trackResult)
            throw runtime_error("Invalid track number");
        pos += trackResult->length;
        uint64_t trackNumber = trackResult->value;

        // Timecode (2 bytes, big-endian, signed)
        if (pos + 2 > data.size())
            throw runtime_error("Incomplete timecode");
        auto timecode =
                static_cast<int16_t>((static_cast<uint16_t>(data[pos]) << 8) | static_cast<uint16_t>(data[pos + 1]));
        pos += 2;

        // Flags
        if (pos >= data.size())
            throw runtime_error("Missing flags byte");
        auto flags = static_cast<uint8_t>(data[pos++]);

        SimpleBlock block;
        block.trackNumber = trackNumber;
        block.timecode    = timecode;
        block.keyframe    = isSimpleBlock ? !!(flags & 0x80) : true; // plain Block: bit is reserved
        block.invisible   = !!(flags & 0x08);
        block.discardable = isSimpleBlock && !!(flags & 0x01);

        uint8_t lacingType = (flags & 0x06) >> 1;
        block.lacing       = static_cast<LacingType>(lacingType);

        // Read frames
        if (block.lacing == LacingType::None)
        {
            // Single frame
            Frame frame;
            frame.data = vector(data.begin() + static_cast<long>(pos), data.end());
            block.frames.push_back(std::move(frame));
            return block;
        }

        // Lacing: read frame count
        if (pos >= data.size())
            throw runtime_error("Missing lacing frame count");
        size_t frameCount = static_cast<size_t>(data[pos++]) + 1;

        // Read frame sizes
        vector<size_t> sizes;
        sizes.reserve(frameCount);

        if (block.lacing == LacingType::Fixed)
        {
            // All frames same size
            const size_t totalRemaining = data.size() - pos;
            if (totalRemaining % frameCount != 0)
                throw runtime_error("Fixed lacing: total size not divisible by frame count");
            const size_t frameSize = totalRemaining / frameCount;
            sizes.assign(frameCount, frameSize);
        } else if (block.lacing == LacingType::Xiph)
        {
            // Xiph lacing: read sizes with 0xFF continuation
            size_t totalSize = 0;
            for (size_t i = 0; i < frameCount - 1; ++i)
            {
                size_t size = 0;
                while (pos < data.size())
                {
                    auto b = static_cast<uint8_t>(data[pos++]);
                    size += b;
                    if (b != 0xFF)
                        break;
                }
                sizes.push_back(size);
                totalSize += size;
            }
            // Last frame size = remaining data
            sizes.push_back(data.size() - pos - totalSize);
        } else if (block.lacing == LacingType::EBML)
        {
            // EBML lacing: sizes are VINTs
            size_t totalSize = 0;
            for (size_t i = 0; i < frameCount - 1; ++i)
            {
                const auto sizeResult = decode_size(data.subspan(pos));
                if (!sizeResult)
                    throw runtime_error("Invalid EBML lacing size");
                pos += sizeResult->length;
                auto size = static_cast<size_t>(sizeResult->value);
                sizes.push_back(size);
                totalSize += size;
            }
            // Last frame size = remaining data
            sizes.push_back(data.size() - pos - totalSize);
        }

        // Read frame data
        block.frames.reserve(frameCount);
        for (size_t i = 0; i < frameCount; ++i)
        {
            if (pos + sizes[i] > data.size())
                throw runtime_error("Frame extends past data end");
            Frame frame;
            frame.data = vector<EBML::byte>(data.begin() + static_cast<long>(pos),
                                            data.begin() + static_cast<long>(pos) + static_cast<long>(sizes[i]));
            block.frames.push_back(std::move(frame));
            pos += sizes[i];
        }

        return block;
    }

    // BlockGroup (Block + additional metadata) is defined in Track.hpp, which
    // is included by every consumer of this file's encode_block(); keeping a
    // single definition there avoids a conflicting redefinition here.

    // Encode a Block (not SimpleBlock) for use in BlockGroup
    inline vector<EBML::byte> encode_block(const Block &block)
    {
        // Blocks are encoded the same as SimpleBlocks but without the keyframe/discardable flags
        SimpleBlock temp;
        temp.trackNumber = block.trackNumber;
        temp.timecode    = block.timecode;
        temp.invisible   = block.invisible;
        temp.lacing      = block.lacing;
        temp.frames      = block.frames;
        temp.keyframe    = false; // plain Block: 0x80 is reserved, keyframe-ness lives in the BlockGroup
        temp.discardable = false;
        return encode_simple_block(temp);
    }

} // namespace SF::Matroska
