#pragma once
#include <cstdio>
#include "../Containers/Span.hpp"

namespace SFTL
{
    template<typename CharT>
    class basic_ostream;

    using ostream = basic_ostream<char>;

    template<typename CharT>
    class basic_ostream
    {
    public:
        using char_type = CharT;

        explicit basic_ostream(void (*output_fn)(const CharT *, size_type)) : output_(output_fn) {}

        basic_ostream &operator<<(const CharT *s)
        {
            size_type len = 0;
            while (s[len] != CharT('\0'))
                ++len;
            if (output_)
                output_(s, len);
            return *this;
        }

        basic_ostream &operator<<(CharT c)
        {
            if (output_)
                output_(&c, 1);
            return *this;
        }

        basic_ostream &operator<<(int val)
        {
            char buf[32];
            int len = std::snprintf(buf, sizeof(buf), "%d", val);
            if (len > 0 && output_)
                output_(buf, static_cast<size_type>(len));
            return *this;
        }

        basic_ostream &operator<<(unsigned int val)
        {
            char buf[32];
            int len = std::snprintf(buf, sizeof(buf), "%u", val);
            if (len > 0 && output_)
                output_(buf, static_cast<size_type>(len));
            return *this;
        }

        basic_ostream &operator<<(long val)
        {
            char buf[32];
            int len = std::snprintf(buf, sizeof(buf), "%ld", val);
            if (len > 0 && output_)
                output_(buf, static_cast<size_type>(len));
            return *this;
        }

        basic_ostream &operator<<(double val)
        {
            char buf[64];
            int len = std::snprintf(buf, sizeof(buf), "%g", val);
            if (len > 0 && output_)
                output_(buf, static_cast<size_type>(len));
            return *this;
        }

        // Support standard manipulators like endl
        basic_ostream &operator<<(basic_ostream &(*func)(basic_ostream &) ) { return func(*this); }

    private:
        void (*output_)(const CharT *, size_type);
    };

    // Standard endl manipulator
    inline ostream &endl(ostream &os)
    {
        os << '\n';
        return os;
    }

    // spanstream: writes formatted data directly into an SFTL span buffer
    class spanstream
    {
    public:
        explicit spanstream(span<char> buffer) : buf_(buffer), pos_(0) {}

        spanstream &operator<<(const char *s)
        {
            while (*s != '\0' && pos_ < buf_.size())
            {
                buf_[pos_++] = *s++;
            }
            return *this;
        }

        spanstream &operator<<(char c)
        {
            if (pos_ < buf_.size())
            {
                buf_[pos_++] = c;
            }
            return *this;
        }

        [[nodiscard]] span<char> span_view() const noexcept { return buf_.First(pos_); }

    private:
        span<char> buf_;
        size_type pos_;
    };

    // Global cout wrapper targeting stdout
    inline ostream &get_cout()
    {
        static ostream cout_instance(
                [](const char *data, size_type size)
                {
                    std::fwrite(data, 1, size, stdout);
                    std::fflush(stdout);
                });
        return cout_instance;
    }

#define cout ::SFTL::get_cout()
} // namespace SFTL
