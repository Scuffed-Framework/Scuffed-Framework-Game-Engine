// Copyright 2006 Nemanja Trifunovic
// SPDX-License-Identifier: MIT
// utf8cpp 4.0.1 : vendored
#pragma once
#include <stdexcept>
#include <iterator>
#include <cstdint>

namespace utf8
{

    // Exceptions
    struct invalid_code_point : std::exception
    {
        uint32_t cp;
        explicit invalid_code_point(uint32_t cp) : cp(cp) {}
        const char *what() const noexcept override { return "Invalid code point"; }
    };
    struct invalid_utf8 : std::exception
    {
        uint8_t u8;
        explicit invalid_utf8(uint8_t u) : u8(u) {}
        const char *what() const noexcept override { return "Invalid UTF-8"; }
    };
    struct invalid_utf16 : std::exception
    {
        uint16_t u16;
        explicit invalid_utf16(uint16_t u) : u16(u) {}
        const char *what() const noexcept override { return "Invalid UTF-16"; }
    };
    struct not_enough_room : std::exception
    {
        const char *what() const noexcept override { return "Not enough room"; }
    };

    namespace internal
    {

        // Unicode constants
        constexpr uint32_t LEAD_SURROGATE_MIN = 0xd800u;
        constexpr uint32_t LEAD_SURROGATE_MAX = 0xdbffu;
        constexpr uint32_t TRAIL_SURROGATE_MIN = 0xdc00u;
        constexpr uint32_t TRAIL_SURROGATE_MAX = 0xdfffu;
        constexpr uint32_t LEAD_OFFSET = LEAD_SURROGATE_MIN - (0x10000 >> 10);
        constexpr uint32_t SURROGATE_OFFSET = 0x10000u - (LEAD_SURROGATE_MIN << 10) - TRAIL_SURROGATE_MIN;
        constexpr uint32_t CODE_POINT_MAX = 0x0010ffffu;

        template <typename octet_type>
        inline uint8_t mask8(octet_type oc) { return static_cast<uint8_t>(0xff & oc); }
        template <typename u16_type>
        inline uint16_t mask16(u16_type oc) { return static_cast<uint16_t>(0xffff & oc); }
        template <typename octet_type>
        inline bool is_trail(octet_type oc) { return (mask8(oc) >> 6) == 0x2; }
        inline bool is_lead_surrogate(uint32_t cp) { return cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX; }
        inline bool is_trail_surrogate(uint32_t cp) { return cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX; }
        inline bool is_surrogate(uint32_t cp) { return cp >= LEAD_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX; }
        inline bool is_code_point_valid(uint32_t cp) { return cp <= CODE_POINT_MAX && !is_surrogate(cp); }

        template <typename octet_iterator>
        inline typename std::iterator_traits<octet_iterator>::difference_type
        sequence_length(octet_iterator lead_it)
        {
            uint8_t lead = mask8(*lead_it);
            if (lead < 0x80)
                return 1;
            else if ((lead >> 5) == 0x6)
                return 2;
            else if ((lead >> 4) == 0xe)
                return 3;
            else if ((lead >> 3) == 0x1e)
                return 4;
            else
                return 0;
        }

        inline bool is_overlong_sequence(uint32_t cp, std::size_t length)
        {
            if (cp < 0x80)
                return length != 1;
            if (cp < 0x800)
                return length != 2;
            if (cp < 0x10000)
                return length != 3;
            return false;
        }

        enum utf_error
        {
            UTF8_OK,
            NOT_ENOUGH_ROOM,
            INVALID_LEAD,
            INCOMPLETE_SEQUENCE,
            OVERLONG_SEQUENCE,
            INVALID_CODE_POINT
        };

        template <typename octet_iterator>
        utf_error validate_next(octet_iterator &it, octet_iterator end, uint32_t &code_point)
        {
            if (it == end)
                return NOT_ENOUGH_ROOM;
            uint32_t cp = mask8(*it);
            auto length = sequence_length(it);
            if (length == 0)
                return INVALID_LEAD;
            if (std::distance(it, end) < static_cast<typename std::iterator_traits<octet_iterator>::difference_type>(length))
                return NOT_ENOUGH_ROOM;
            for (std::size_t i = 1; i < length; ++i)
            {
                cp = ((cp << 6) & 0xffffffff) + (mask8(*(it + i)) & 0x3f);
                if (!is_trail(*(it + i)))
                    return INCOMPLETE_SEQUENCE;
            }
            // remove the header bits
            switch (length)
            {
            case 2:
                cp -= 0x3080u;
                break;
            case 3:
                cp -= 0xe2080u;
                break;
            case 4:
                cp -= 0x3c82080u;
                break;
            }
            if (!is_code_point_valid(cp))
                return INVALID_CODE_POINT;
            if (is_overlong_sequence(cp, length))
                return OVERLONG_SEQUENCE;
            code_point = cp;
            std::advance(it, length);
            return UTF8_OK;
        }

        template <typename octet_iterator>
        utf_error validate_next(octet_iterator &it, octet_iterator end)
        {
            uint32_t ignored;
            return validate_next(it, end, ignored);
        }

        template <typename octet_iterator>
        octet_iterator prior(octet_iterator it, octet_iterator start)
        {
            while (is_trail(*(--it)) && it != start)
            {
            }
            return it;
        }

    } // namespace internal

    // checked namespace : throws on error
    namespace checked
    {

        template <typename octet_iterator>
        uint32_t next(octet_iterator &it, octet_iterator end)
        {
            uint32_t cp = 0;
            auto err = internal::validate_next(it, end, cp);
            switch (err)
            {
            case internal::UTF8_OK:
                break;
            case internal::NOT_ENOUGH_ROOM:
                throw not_enough_room();
            case internal::INVALID_LEAD:
            case internal::INCOMPLETE_SEQUENCE:
            case internal::OVERLONG_SEQUENCE:
                throw invalid_utf8(static_cast<uint8_t>(*it));
            case internal::INVALID_CODE_POINT:
                throw invalid_code_point(cp);
            }
            return cp;
        }

        template <typename octet_iterator>
        uint32_t peek_next(octet_iterator it, octet_iterator end)
        {
            return next(it, end);
        }

        template <typename octet_iterator>
        uint32_t prior(octet_iterator &it, octet_iterator start)
        {
            octet_iterator end = it;
            while (internal::is_trail(*(--it)) && it != start)
            {
            }
            uint32_t cp = peek_next(it, end);
            return cp;
        }

        template <typename output_iterator>
        output_iterator append(uint32_t cp, output_iterator result)
        {
            if (!internal::is_code_point_valid(cp))
                throw invalid_code_point(cp);
            if (cp < 0x80)
                *result++ = static_cast<uint8_t>(cp);
            else if (cp < 0x800)
            {
                *result++ = static_cast<uint8_t>((cp >> 6) | 0xc0);
                *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
            }
            else if (cp < 0x10000)
            {
                *result++ = static_cast<uint8_t>((cp >> 12) | 0xe0);
                *result++ = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
                *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
            }
            else
            {
                *result++ = static_cast<uint8_t>((cp >> 18) | 0xf0);
                *result++ = static_cast<uint8_t>(((cp >> 12) & 0x3f) | 0x80);
                *result++ = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
                *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
            }
            return result;
        }

    } // namespace checked

    // Convenience wrappers in utf8:: root (delegates to checked)
    template <typename output_iterator>
    output_iterator append(uint32_t cp, output_iterator result)
    {
        return checked::append(cp, result);
    }

    template <typename octet_iterator>
    uint32_t next(octet_iterator &it, octet_iterator end)
    {
        return checked::next(it, end);
    }

    template <typename octet_iterator>
    uint32_t peek_next(octet_iterator it, octet_iterator end)
    {
        return checked::peek_next(it, end);
    }

    template <typename u16bit_iterator, typename octet_iterator>
    octet_iterator utf16to8(u16bit_iterator start, u16bit_iterator end, octet_iterator result)
    {
        while (start != end)
        {
            uint32_t cp = internal::mask16(*start++);
            if (internal::is_lead_surrogate(cp))
            {
                if (start != end)
                {
                    uint32_t trail = internal::mask16(*start++);
                    if (internal::is_trail_surrogate(trail))
                        cp = (cp << 10) + trail + internal::SURROGATE_OFFSET;
                    else
                        throw invalid_utf16(static_cast<uint16_t>(trail));
                }
                else
                    throw invalid_utf16(static_cast<uint16_t>(cp));
            }
            else if (internal::is_trail_surrogate(cp))
                throw invalid_utf16(static_cast<uint16_t>(cp));
            result = checked::append(cp, result);
        }
        return result;
    }

    template <typename u32bit_iterator, typename octet_iterator>
    octet_iterator utf32to8(u32bit_iterator start, u32bit_iterator end, octet_iterator result)
    {
        while (start != end)
        {
            uint32_t cp = static_cast<uint32_t>(*start++);
            result = checked::append(cp, result);
        }
        return result;
    }

} // namespace utf8
