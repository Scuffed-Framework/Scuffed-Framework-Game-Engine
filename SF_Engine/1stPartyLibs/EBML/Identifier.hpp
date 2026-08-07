#pragma once

#include "VINT.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace SF::EBML
{
    class Identifier
    {
    public:
        constexpr Identifier() = default;

        // `rawWithMarker` must be a value as it appears on the wire, e.g.
        // 0x1A45DFA3 for \EBML, or 0xBF for \CRC32.
        constexpr explicit Identifier(std::uint32_t rawWithMarker)
            : m_value(rawWithMarker), m_length(length_of(rawWithMarker))
        {
        }

        friend constexpr bool operator==(const Identifier &, const Identifier &) = default;

        constexpr std::uint32_t value() const noexcept { return m_value; }
        constexpr std::uint8_t length() const noexcept { return m_length; }

        std::vector<byte> encode() const { return encode_id(m_value, m_length); }

        static std::optional<Identifier> decode(std::span<const byte> data)
        {
            auto v = decode_id(data);
            if (!v)
                return std::nullopt;
            Identifier id;
            id.m_value = v->value;
            id.m_length = v->length;
            return id;
        }

    private:
        static constexpr std::uint8_t length_of(std::uint32_t raw)
        {
            // Find the most-significant non-zero byte; its marker bit gives length.
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                byte b = static_cast<byte>((raw >> shift) & 0xFF);
                if (b != 0)
                {
                    std::uint8_t l = VintValidator::vint_length(b);
                    return l == 0 ? 1 : l;
                }
            }
            return 1; // raw == 0 is technically invalid; treat as length 1
        }

        std::uint32_t m_value = 0;
        std::uint8_t m_length = 1;
    };

    // A few well-known top-level EBML header IDs (RFC 8794 §11), handy as
    // defaults and for the example/tests. Application-specific IDs (e.g. all of
    // Matroska's) belong in the consuming project's own Schema, not here.
    namespace ids
    {
        inline constexpr Identifier EBML{0x1A45DFA3};
        inline constexpr Identifier EBMLVersion{0x4286};
        inline constexpr Identifier EBMLReadVersion{0x42F7};
        inline constexpr Identifier EBMLMaxIDLength{0x42F2};
        inline constexpr Identifier EBMLMaxSizeLength{0x42F3};
        inline constexpr Identifier DocType{0x4282};
        inline constexpr Identifier DocTypeVersion{0x4287};
        inline constexpr Identifier DocTypeReadVersion{0x4285};
        inline constexpr Identifier Void{0xEC};
        inline constexpr Identifier CRC32{0xBF};
    } // namespace ids

} // namespace SF::EBML