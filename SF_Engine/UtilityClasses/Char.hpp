/******************************************************************************/
/* Char.hpp                                                     */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin.                                         */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
// UtilityClasses/Char.hpp - A file containing char utilities.
#include <cstddef>
#include <type_traits>

namespace SF::Engine
{
    template <typename T, std::size_t N>
    constexpr std::size_t size(const T (&)[N]) noexcept
    {
        return N;
    }

    // Character classification utilities
    constexpr bool is_alpha(char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    constexpr bool is_digit(char c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    constexpr bool is_alnum(char c) noexcept
    {
        return is_alpha(c) || is_digit(c);
    }

    constexpr bool is_space(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    constexpr bool is_upper(char c) noexcept
    {
        return c >= 'A' && c <= 'Z';
    }

    constexpr bool is_lower(char c) noexcept
    {
        return c >= 'a' && c <= 'z';
    }

    constexpr bool is_hex(char c) noexcept
    {
        return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    constexpr bool is_printable(char c) noexcept
    {
        return c >= 32 && c <= 126;
    }

    // Character conversion utilities
    constexpr char to_upper(char c) noexcept
    {
        return is_lower(c) ? c - ('a' - 'A') : c;
    }

    constexpr char to_lower(char c) noexcept
    {
        return is_upper(c) ? c + ('a' - 'A') : c;
    }

    // String length (constexpr)
    constexpr std::size_t strlen(const char *str) noexcept
    {
        std::size_t len = 0;
        while (str[len] != '\0')
            ++len;
        return len;
    }

    // String comparison
    constexpr int strcmp(const char *s1, const char *s2) noexcept
    {
        while (*s1 && (*s1 == *s2))
        {
            ++s1;
            ++s2;
        }
        return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
    }

    constexpr int strncmp(const char *s1, const char *s2, std::size_t n) noexcept
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            if (s1[i] != s2[i])
                return static_cast<unsigned char>(s1[i]) - static_cast<unsigned char>(s2[i]);
            if (s1[i] == '\0')
                return 0;
        }
        return 0;
    }

    // Case-insensitive comparison
    constexpr int stricmp(const char *s1, const char *s2) noexcept
    {
        while (*s1 && (to_lower(*s1) == to_lower(*s2)))
        {
            ++s1;
            ++s2;
        }
        return to_lower(*s1) - to_lower(*s2);
    }

    // String search
    constexpr const char *strchr(const char *str, char ch) noexcept
    {
        while (*str)
        {
            if (*str == ch)
                return str;
            ++str;
        }
        return ch == '\0' ? str : nullptr;
    }

    constexpr const char *strrchr(const char *str, char ch) noexcept
    {
        const char *last = nullptr;
        while (*str)
        {
            if (*str == ch)
                last = str;
            ++str;
        }
        return (ch == '\0') ? str : last;
    }

    constexpr const char *strstr(const char *haystack, const char *needle) noexcept
    {
        if (*needle == '\0')
            return haystack;

        for (const char *h = haystack; *h; ++h)
        {
            const char *h_ptr = h;
            const char *n_ptr = needle;

            while (*h_ptr && *n_ptr && (*h_ptr == *n_ptr))
            {
                ++h_ptr;
                ++n_ptr;
            }

            if (*n_ptr == '\0')
                return h;
        }
        return nullptr;
    }

    // Character to digit conversion
    constexpr int char_to_digit(char c) noexcept
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    }

    constexpr char digit_to_char(int digit, bool uppercase = true) noexcept
    {
        if (digit < 0 || digit > 35)
            return '\0';
        if (digit < 10)
            return '0' + digit;
        return (uppercase ? 'A' : 'a') + (digit - 10);
    }

    // Memory operations
    constexpr void *memset(void *dest, int ch, std::size_t count) noexcept
    {
        unsigned char *d = static_cast<unsigned char *>(dest);
        unsigned char value = static_cast<unsigned char>(ch);
        for (std::size_t i = 0; i < count; ++i)
            d[i] = value;
        return dest;
    }

    constexpr void *memcpy(void *dest, const void *src, std::size_t count) noexcept
    {
        unsigned char *d = static_cast<unsigned char *>(dest);
        const unsigned char *s = static_cast<const unsigned char *>(src);
        for (std::size_t i = 0; i < count; ++i)
            d[i] = s[i];
        return dest;
    }

    constexpr int memcmp(const void *lhs, const void *rhs, std::size_t count) noexcept
    {
        const unsigned char *l = static_cast<const unsigned char *>(lhs);
        const unsigned char *r = static_cast<const unsigned char *>(rhs);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (l[i] != r[i])
                return l[i] - r[i];
        }
        return 0;
    }
}