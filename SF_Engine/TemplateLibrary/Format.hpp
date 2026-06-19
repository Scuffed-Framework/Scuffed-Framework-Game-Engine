/******************************************************************************/
/* Format.hpp                                                                 */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin (the name I was assigned in french class).                                         */
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
#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <cstdio>
#include <memory>
#include <cmath>

// std::format but not shit
namespace SFTL
{
    namespace Detail
    {
        // Helper to determine buffer size needed for snprintf
        template <typename... Args>
        inline int GetFormattedSize(const char *fmt, Args &&...args)
        {
            // First call with nullptr to get required size
            int size = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
            return size;
        }

        // Format using snprintf (faster than streams, works everywhere)
        template <typename... Args>
        inline std::string FormatImpl(const char *fmt, Args &&...args)
        {
            int size = GetFormattedSize(fmt, std::forward<Args>(args)...);
            if (size < 0)
                return ""; // Error

            // +1 for null terminator
            std::string result(size + 1, '\0');
            std::snprintf(&result[0], size + 1, fmt, std::forward<Args>(args)...);
            result.resize(size); // Remove null terminator

            return result;
        }

        // Stream-based fallback for non-POD types
        template <typename T>
        inline std::string ToString(const T &value)
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        // Specializations for common types with formatting control
        inline std::string ToString(float value, int precision = 6)
        {
            return FormatImpl("%.*f", precision, value);
        }

        inline std::string ToString(double value, int precision = 6)
        {
            return FormatImpl("%.*f", precision, value);
        }

        inline std::string ToString(long double value, int precision = 6)
        {
            return FormatImpl("%.*Lf", precision, value);
        }

        inline std::string ToString(int value)
        {
            return FormatImpl("%d", value);
        }

        inline std::string ToString(long value)
        {
            return FormatImpl("%ld", value);
        }

        inline std::string ToString(long long value)
        {
            return FormatImpl("%lld", value);
        }

        inline std::string ToString(unsigned int value)
        {
            return FormatImpl("%u", value);
        }

        inline std::string ToString(unsigned long value)
        {
            return FormatImpl("%lu", value);
        }

        inline std::string ToString(unsigned long long value)
        {
            return FormatImpl("%llu", value);
        }

        inline std::string ToString(const char *value)
        {
            return value ? std::string(value) : "";
        }

        inline std::string ToString(const std::string &value)
        {
            return value;
        }

        inline std::string ToString(bool value)
        {
            return value ? "true" : "false";
        }

        inline std::string ToString(char value)
        {
            return std::string(1, value);
        }
    }

    // Simple format function that works everywhere
    // Usage: Format("Value: %d, Name: %s", 42, "test")
    template <typename... Args>
    inline std::string Format(const char *fmt, Args &&...args)
    {
        return Detail::FormatImpl(fmt, std::forward<Args>(args)...);
    }

    // Type-safe formatting with precision control
    namespace Fmt
    {
        // Fixed precision floating point
        struct Fixed
        {
            double value;
            int precision;

            Fixed(double v, int p = 3) : value(v), precision(p) {}
            Fixed(float v, int p = 3) : value(v), precision(p) {}
        };

        inline std::string ToString(const Fixed &f)
        {
            return Detail::FormatImpl("%.*f", f.precision, f.value);
        }

        // Scientific notation
        struct Scientific
        {
            double value;
            int precision;

            Scientific(double v, int p = 3) : value(v), precision(p) {}
            Scientific(float v, int p = 3) : value(v), precision(p) {}
        };

        inline std::string ToString(const Scientific &s)
        {
            return Detail::FormatImpl("%.*e", s.precision, s.value);
        }

        // Hexadecimal
        struct Hex
        {
            unsigned long long value;
            bool uppercase;
            bool prefix;

            Hex(unsigned long long v, bool upper = false, bool pre = true)
                : value(v), uppercase(upper), prefix(pre) {}

            template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            Hex(T v, bool upper = false, bool pre = true)
                : value(static_cast<unsigned long long>(v)), uppercase(upper), prefix(pre) {}
        };

        inline std::string ToString(const Hex &h)
        {
            if (h.prefix)
            {
                return h.uppercase
                           ? Detail::FormatImpl("0x%llX", h.value)
                           : Detail::FormatImpl("0x%llx", h.value);
            }
            else
            {
                return h.uppercase
                           ? Detail::FormatImpl("%llX", h.value)
                           : Detail::FormatImpl("%llx", h.value);
            }
        }

        // Binary representation
        struct Binary
        {
            unsigned long long value;
            bool prefix;

            Binary(unsigned long long v, bool pre = true)
                : value(v), prefix(pre) {}

            template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            Binary(T v, bool pre = true)
                : value(static_cast<unsigned long long>(v)), prefix(pre) {}
        };

        inline std::string ToString(const Binary &b)
        {
            if (b.value == 0)
                return b.prefix ? "0b0" : "0";

            std::string result;
            unsigned long long val = b.value;

            while (val > 0)
            {
                result = (val & 1 ? '1' : '0') + result;
                val >>= 1;
            }

            return b.prefix ? "0b" + result : result;
        }

        // Width and padding
        struct Padded
        {
            std::string value;
            size_t width;
            char fillChar;
            bool leftAlign;

            Padded(const std::string &v, size_t w, char fill = ' ', bool left = false)
                : value(v), width(w), fillChar(fill), leftAlign(left) {}
        };

        inline std::string ToString(const Padded &p)
        {
            if (p.value.length() >= p.width)
                return p.value;

            size_t padding = p.width - p.value.length();

            if (p.leftAlign)
                return p.value + std::string(padding, p.fillChar);
            else
                return std::string(padding, p.fillChar) + p.value;
        }
    }

    // String builder for efficient concatenation
    class StringBuilder
    {
    public:
        StringBuilder() = default;

        template <typename T>
        StringBuilder &Append(const T &value)
        {
            stream_ << value;
            return *this;
        }

        StringBuilder &Append(float value, int precision = 6)
        {
            stream_ << std::fixed << std::setprecision(precision) << value;
            return *this;
        }

        StringBuilder &Append(double value, int precision = 6)
        {
            stream_ << std::fixed << std::setprecision(precision) << value;
            return *this;
        }

        StringBuilder &Append(const Fmt::Fixed &f)
        {
            stream_ << std::fixed << std::setprecision(f.precision) << f.value;
            return *this;
        }

        StringBuilder &Append(const Fmt::Scientific &s)
        {
            stream_ << std::scientific << std::setprecision(s.precision) << s.value;
            return *this;
        }

        StringBuilder &Append(const Fmt::Hex &h)
        {
            stream_ << Fmt::ToString(h);
            return *this;
        }

        StringBuilder &Append(const Fmt::Binary &b)
        {
            stream_ << Fmt::ToString(b);
            return *this;
        }

        StringBuilder &Append(const Fmt::Padded &p)
        {
            stream_ << Fmt::ToString(p);
            return *this;
        }

        template <typename T>
        StringBuilder &operator<<(const T &value)
        {
            return Append(value);
        }

        std::string ToString() const
        {
            return stream_.str();
        }

        void Clear()
        {
            stream_.str("");
            stream_.clear();
        }

        size_t Length() const
        {
            return stream_.str().length();
        }

        bool Empty() const
        {
            return stream_.str().empty();
        }

    private:
        std::ostringstream stream_;
    };

    // Variadic string builder with separators
    namespace Detail
    {
        inline void JoinImpl(StringBuilder &, const char *) {}

        template <typename T, typename... Args>
        inline void JoinImpl(StringBuilder &sb, const char *sep, const T &first, Args &&...rest)
        {
            sb.Append(first);
            if constexpr (sizeof...(rest) > 0)
            {
                sb.Append(sep);
                JoinImpl(sb, sep, std::forward<Args>(rest)...);
            }
        }
    }

    // Join multiple values with separator
    template <typename... Args>
    inline std::string Join(const char *separator, Args &&...args)
    {
        StringBuilder sb;
        Detail::JoinImpl(sb, separator, std::forward<Args>(args)...);
        return sb.ToString();
    }

    // Convenience functions
    template <typename T>
    inline std::string ToHexString(T value, bool uppercase = false, bool prefix = true)
    {
        return Fmt::ToString(Fmt::Hex(value, uppercase, prefix));
    }

    template <typename T>
    inline std::string ToBinaryString(T value, bool prefix = true)
    {
        return Fmt::ToString(Fmt::Binary(value, prefix));
    }

    // Format time duration (works with your Time class)
    template <typename DurationType>
    inline std::string FormatDuration(const DurationType &duration)
    {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        auto abs_us = std::abs(microseconds);

        if (abs_us >= 1'000'000) // >= 1 second
        {
            double seconds = microseconds / 1'000'000.0;
            return Detail::FormatImpl("%.3fs", seconds);
        }
        else if (abs_us >= 1'000) // >= 1 millisecond
        {
            double milliseconds = microseconds / 1'000.0;
            return Detail::FormatImpl("%.3fms", milliseconds);
        }
        else // < 1 millisecond
        {
            return Detail::FormatImpl("%lldμs", static_cast<long long>(microseconds));
        }
    }

    // Size formatting (bytes to human-readable)
    inline std::string FormatBytes(size_t bytes, int precision = 2)
    {
        const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unitIndex < 5)
        {
            size /= 1024.0;
            ++unitIndex;
        }

        if (unitIndex == 0)
            return Detail::FormatImpl("%zu%s", bytes, units[0]);
        else
            return Detail::FormatImpl("%.*f%s", precision, size, units[unitIndex]);
    }

    // Format percentage
    inline std::string FormatPercent(double value, int precision = 1)
    {
        return Detail::FormatImpl("%.*f%%", precision, value * 100.0);
    }

    // Pad string
    inline std::string PadLeft(const std::string &str, size_t width, char fillChar = ' ')
    {
        return Fmt::ToString(Fmt::Padded(str, width, fillChar, false));
    }

    inline std::string PadRight(const std::string &str, size_t width, char fillChar = ' ')
    {
        return Fmt::ToString(Fmt::Padded(str, width, fillChar, true));
    }
}