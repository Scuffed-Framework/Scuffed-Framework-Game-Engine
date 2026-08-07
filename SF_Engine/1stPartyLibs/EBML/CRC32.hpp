#pragma once

#include <cstdint>
#include <span>

namespace SF::EBML
{
    // IEEE 802.3 CRC-32 (the same polynomial zlib/PNG/gzip use), which is what
    // the EBML \CRC32 element specifies.
    inline std::uint32_t crc32(std::span<const std::uint8_t> data)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::uint8_t byte : data)
        {
            crc ^= byte;
            for (int i = 0; i < 8; ++i)
            {
                std::uint32_t mask = static_cast<std::uint32_t>(-static_cast<std::int32_t>(crc & 1));
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }
        return crc ^ 0xFFFFFFFFu;
    }

} // namespace SF::EBML