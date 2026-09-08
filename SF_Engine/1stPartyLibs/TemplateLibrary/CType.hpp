/******************************************************************************/
/* Ctype.hpp                                                                  */
/******************************************************************************/
/*            This file is part of                                            */
/*            Scuffed Framework Standard Template Library                     */
/******************************************************************************/
#pragma once

#include "Char.hpp"

namespace SFTL
{
    struct ctype_base
    {
        using mask = unsigned short;

        static constexpr mask space  = 1 << 0;
        static constexpr mask print  = 1 << 1;
        static constexpr mask cntrl  = 1 << 2;
        static constexpr mask upper  = 1 << 3;
        static constexpr mask lower  = 1 << 4;
        static constexpr mask alpha  = 1 << 5;
        static constexpr mask digit  = 1 << 6;
        static constexpr mask punct  = 1 << 7;
        static constexpr mask xdigit = 1 << 8;
        static constexpr mask blank  = 1 << 9;
        static constexpr mask alnum  = alpha | digit;
        static constexpr mask graph  = alnum | punct;
    };

    template<typename CharT>
    class ctype : public ctype_base
    {
    public:
        using char_type = CharT;

        explicit ctype(size_type /*refs*/ = 0) noexcept {}

        bool is(mask m, char_type c) const noexcept
        {
            unsigned char uc = static_cast<unsigned char>(c);
            return (m & table_size(uc)) != 0;
        }

        const char_type *is(const char_type *low, const char_type *high, mask *vec) const noexcept
        {
            while (low < high)
            {
                *vec++ = table_size(static_cast<unsigned char>(*low++));
            }
            return high;
        }

        const char_type *scan_is(mask m, const char_type *low, const char_type *high) const noexcept
        {
            while (low < high && !is(m, *low))
                ++low;
            return low;
        }

        const char_type *scan_not(mask m, const char_type *low, const char_type *high) const noexcept
        {
            while (low < high && is(m, *low))
                ++low;
            return low;
        }

        char_type toupper(char_type c) const noexcept
        {
            return static_cast<char_type>(SFTL::to_upper(static_cast<char>(c)));
        }

        const char_type *toupper(char_type *low, const char_type *high) const noexcept
        {
            while (low < high)
            {
                *low = SFTL::to_upper(static_cast<char>(*low));
                ++low;
            }
            return high;
        }

        char_type tolower(char_type c) const noexcept
        {
            return static_cast<char_type>(SFTL::to_lower(static_cast<char>(c)));
        }

        const char_type *tolower(char_type *low, const char_type *high) const noexcept
        {
            while (low < high)
            {
                *low = SFTL::to_lower(static_cast<char>(*low));
                ++low;
            }
            return high;
        }

        char_type widen(char c) const noexcept { return static_cast<char_type>(c); }
        const char *widen(const char *low, const char *high, char_type *dest) const noexcept
        {
            while (low < high)
                *dest++ = static_cast<char_type>(*low++);
            return high;
        }

        char narrow(char_type c, char /*dfault*/) const noexcept { return static_cast<char>(c); }
        const char_type *narrow(const char_type *low, const char_type *high, char /*dfault*/, char *dest) const noexcept
        {
            while (low < high)
                *dest++ = static_cast<char>(*low++);
            return high;
        }

    private:
        static constexpr mask table_size(unsigned char c) noexcept
        {
            mask m  = 0;
            char sc = static_cast<char>(c);
            if (SFTL::is_space(sc))
                m |= space;
            if (SFTL::is_printable(sc))
                m |= print;
            if (SFTL::is_upper(sc))
                m |= upper;
            if (SFTL::is_lower(sc))
                m |= lower;
            if (SFTL::is_alpha(sc))
                m |= alpha;
            if (SFTL::is_digit(sc))
                m |= digit;
            if (SFTL::is_hex(sc))
                m |= xdigit;
            if (sc == ' ' || sc == '\t')
                m |= blank;
            if (SFTL::is_printable(sc) && !SFTL::is_alnum(sc) && !SFTL::is_space(sc))
                m |= punct;
            if (c < 32 || c == 127)
                m |= cntrl;
            return m;
        }
    };
} // namespace SFTL
