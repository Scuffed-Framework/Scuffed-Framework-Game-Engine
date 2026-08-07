#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <optional>
#include <stdexcept>
#include <vector>
#include <limits>
#include <array>

namespace SF::EBML
{
    using byte = std::uint8_t;
    inline constexpr std::uint8_t kMaxVintLength = 8;
    inline constexpr std::uint64_t kMaxVintValue = (std::uint64_t{1} << 56) - 2;

    // Thread-safe VINT utilities
    class VintValidator
    {
    public:
        static bool is_valid_first_byte(byte b) noexcept
        {
            return b != 0 && vint_length(b) > 0;
        }

        static constexpr std::uint8_t vint_length(byte firstByte) noexcept
        {
            if (firstByte == 0)
                return 0;
            std::uint8_t length = 1;
            byte mask = 0x80;
            while (!(firstByte & mask) && length < 8)
            {
                mask >>= 1;
                ++length;
            }
            return length;
        }

        static bool would_overflow(std::span<const byte> data) noexcept
        {
            if (data.empty())
                return true;
            
            std::uint8_t len = vint_length(data[0]);
            if (len == 0 || len > kMaxVintLength)
                return true;
            
            if (data.size() < len)
                return true;
                
            // Check if this would exceed max representable value
            if (len == 8)
            {
                // First byte mask for 8-byte vint
                byte mask = 0x01;
                if ((data[0] & ~mask) > 0)
                    return true;
            }
            
            return false;
        }
    };

    struct VintSize
    {
        std::uint64_t value = 0;
        std::uint8_t length = 0;
        bool unknown = false;
        
        bool is_valid() const noexcept { return length > 0 && length <= kMaxVintLength; }
    };

    constexpr std::optional<VintSize> decode_size(std::span<const byte> data) noexcept
    {
        if (data.empty())
            return std::nullopt;

        const std::uint8_t length = VintValidator::vint_length(data[0]);
        if (length == 0 || length > kMaxVintLength || data.size() < length)
            return std::nullopt;

        const byte firstByteMask = static_cast<byte>(0xFFu >> length);
        std::uint64_t value = data[0] & firstByteMask;
        bool allOnes = (value == firstByteMask);

        for (std::uint8_t i = 1; i < length; ++i)
        {
            value = (value << 8) | data[i];
            if (data[i] != 0xFF)
                allOnes = false;
        }

        return VintSize{value, length, allOnes};
    }

    constexpr std::uint64_t max_size_for_length(std::uint8_t length) noexcept
    {
        if (length == 0 || length > kMaxVintLength)
            return 0;
        return (std::uint64_t{1} << (7 * length)) - 2;
    }

    constexpr std::uint8_t minimal_size_length(std::uint64_t value) noexcept
    {
        std::uint8_t length = 1;
        while (length < kMaxVintLength && value > max_size_for_length(length))
            ++length;
        return length;
    }

    inline std::vector<byte> encode_size(std::uint64_t value, std::uint8_t forceLength = 0)
    {
        std::uint8_t length = forceLength;
        if (length == 0)
        {
            length = minimal_size_length(value);
            if (value > max_size_for_length(length))
                throw std::out_of_range("EBML size value exceeds maximum representable size");
        }
        else
        {
            if (length == 0 || length > kMaxVintLength)
                throw std::out_of_range("EBML size vint length must be between 1 and 8");
            if (value > max_size_for_length(length))
                throw std::out_of_range("EBML size value does not fit in requested vint length");
        }

        std::vector<byte> out(length);
        for (std::uint8_t i = 0; i < length; ++i)
            out[length - 1 - i] = static_cast<byte>((value >> (8 * i)) & 0xFF);
        out[0] |= static_cast<byte>(0x80 >> (length - 1));
        return out;
    }

    inline std::vector<byte> encode_unknown_size(std::uint8_t length = 8)
    {
        if (length == 0 || length > kMaxVintLength)
            throw std::out_of_range("EBML unknown-size vint length must be between 1 and 8");
        std::vector<byte> out(length, 0xFF);
        out[0] = static_cast<byte>((0xFFu >> length) | (0x80u >> (length - 1)));
        return out;
    }

    struct VintId
    {
        std::uint32_t value = 0;
        std::uint8_t length = 0;
        
        bool is_valid() const noexcept { return length > 0 && length <= 4; }
    };

    constexpr std::optional<VintId> decode_id(std::span<const byte> data) noexcept
    {
        if (data.empty())
            return std::nullopt;

        const std::uint8_t length = VintValidator::vint_length(data[0]);
        if (length == 0 || length > 4 || data.size() < length)
            return std::nullopt;

        std::uint32_t value = 0;
        for (std::uint8_t i = 0; i < length; ++i)
            value = (value << 8) | data[i];

        return VintId{value, length};
    }

    inline std::vector<byte> encode_id(std::uint32_t idWithMarker, std::uint8_t length)
    {
        if (length > 4)
            throw std::out_of_range("EBML ID length must be between 1 and 4");
        std::vector<byte> out(length);
        for (std::uint8_t i = 0; i < length; ++i)
            out[length - 1 - i] = static_cast<byte>((idWithMarker >> (8 * i)) & 0xFF);
        return out;
    }
}