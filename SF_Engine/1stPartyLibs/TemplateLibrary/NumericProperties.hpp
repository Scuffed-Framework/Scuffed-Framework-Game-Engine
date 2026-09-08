#pragma once
#include "Streams/IOS.hpp"
#include "TypeTraits.hpp"

namespace SFTL
{
    template<typename T>
    struct numeric_limits
    {
        static constexpr bool is_specialized = false;
        static constexpr bool is_signed      = false;
        static constexpr bool is_integer     = false;

        static constexpr T min() noexcept { return T(); }
        static constexpr T max() noexcept { return T(); }
        static constexpr T lowest() noexcept { return T(); }
        static constexpr T epsilon() noexcept { return T(); }
        static constexpr T infinity() noexcept { return T(); }
        static constexpr T quiet_NaN() noexcept { return T(); }
    };

#define SFTL_DEFINE_INT_LIMIT(Type, MinVal, MaxVal, Signed)                                                            \
    template<>                                                                                                         \
    struct numeric_limits<Type>                                                                                        \
    {                                                                                                                  \
        static constexpr bool is_specialized = true;                                                                   \
        static constexpr bool is_signed      = Signed;                                                                 \
        static constexpr bool is_integer     = true;                                                                   \
                                                                                                                       \
        static constexpr Type min() noexcept { return MinVal; }                                                        \
        static constexpr Type max() noexcept { return MaxVal; }                                                        \
        static constexpr Type lowest() noexcept { return MinVal; }                                                     \
    }

    SFTL_DEFINE_INT_LIMIT(char, (-128), 127, true);
    SFTL_DEFINE_INT_LIMIT(signed char, (-128), 127, true);
    SFTL_DEFINE_INT_LIMIT(unsigned char, 0, 255, false);
    SFTL_DEFINE_INT_LIMIT(short, (-32768), 32767, true);
    SFTL_DEFINE_INT_LIMIT(unsigned short, 0, 65535, false);
    SFTL_DEFINE_INT_LIMIT(int, (-2147483647 - 1), 2147483647, true);
    SFTL_DEFINE_INT_LIMIT(unsigned int, 0, 4294967295U, false);
    SFTL_DEFINE_INT_LIMIT(long, (-2147483647L - 1L), 2147483647L, true);
    SFTL_DEFINE_INT_LIMIT(unsigned long, 0UL, 4294967295UL, false);
    SFTL_DEFINE_INT_LIMIT(long long, (-9223372036854775807LL - 1LL), 9223372036854775807LL, true);
    SFTL_DEFINE_INT_LIMIT(unsigned long long, 0ULL, 18446744073709551615ULL, false);

#undef SFTL_DEFINE_INT_LIMIT

    // Floating-point specializations using compiler builtins / standard representations
    template<>
    struct numeric_limits<float>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed      = true;
        static constexpr bool is_integer     = false;

        static constexpr float min() noexcept { return 1.17549435e-38F; }
        static constexpr float max() noexcept { return 3.40282347e+38F; }
        static constexpr float lowest() noexcept { return -3.40282347e+38F; }
        static constexpr float epsilon() noexcept { return 1.19209290e-07F; }
        static constexpr float infinity() noexcept { return __builtin_huge_valf(); }
        static constexpr float quiet_NaN() noexcept { return __builtin_nanf(""); }
    };

    template<>
    struct numeric_limits<double>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed      = true;
        static constexpr bool is_integer     = false;

        static constexpr double min() noexcept { return 2.2250738585072014e-308; }
        static constexpr double max() noexcept { return 1.7976931348623157e+308; }
        static constexpr double lowest() noexcept { return -1.7976931348623157e+308; }
        static constexpr double epsilon() noexcept { return 2.2204460492503131e-16; }
        static constexpr double infinity() noexcept { return __builtin_huge_val(); }
        static constexpr double quiet_NaN() noexcept { return __builtin_nan(""); }
    };

    template<>
    struct numeric_limits<long double>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed      = true;
        static constexpr bool is_integer     = false;

        static constexpr long double min() noexcept { return __LDBL_MIN__; }
        static constexpr long double max() noexcept { return __LDBL_MAX__; }
        static constexpr long double lowest() noexcept { return -__LDBL_MAX__; }
        static constexpr long double epsilon() noexcept { return __LDBL_EPSILON__; }
        static constexpr long double infinity() noexcept { return __builtin_huge_vall(); }
        static constexpr long double quiet_NaN() noexcept { return __builtin_nanl(""); }
    };

    namespace Detail
    {
        template<typename T>
        struct __numeric_traits
        {
            using limits = numeric_limits<T>;

            static constexpr bool __is_signed = limits::is_signed;
            static constexpr T __min          = limits::min();
            static constexpr T __max          = limits::max();
        };
    } // namespace Detail

    template<typename CharT, typename InputIt>
    class num_get
    {
    public:
        using char_type = CharT;
        using iter_type = InputIt;

        explicit num_get(size_t /*refs*/ = 0) noexcept {}

        template<typename ValType>
        iter_type get(iter_type beg, iter_type end, ios_base & /*io*/, ios_base::iostate &err, ValType &val) const
        {
            using Traits = char_traits<CharT>;
            if (beg == end)
            {
                err |= ios_base::eofbit | ios_base::failbit;
                return beg;
            }

            // Simple parsing loop for integers/floating point strings
            long long accum  = 0;
            bool negative    = false;
            bool found_digit = false;

            CharT c = *beg;
            if (Traits::eq(c, CharT('-')))
            {
                negative = true;
                ++beg;
            } else if (Traits::eq(c, CharT('+')))
            {
                ++beg;
            }

            while (beg != end)
            {
                c         = *beg;
                int digit = SFTL::char_to_digit(static_cast<char>(c));
                if (digit < 0)
                    break;

                found_digit = true;
                accum       = accum * 10 + digit;
                ++beg;
            }

            if (!found_digit)
            {
                err |= ios_base::failbit;
            } else
            {
                val = static_cast<ValType>(negative ? -accum : accum);
            }

            if (beg == end)
            {
                err |= ios_base::eofbit;
            }

            return beg;
        }

        // Overloads mapped to standard signature specifiers
        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, bool &val) const
        {
            // Basic boolean handling
            if (beg == end)
            {
                err |= ios_base::eofbit | ios_base::failbit;
                return beg;
            }
            CharT c = *beg;
            if (c == CharT('0'))
            {
                val = false;
                ++beg;
                return beg;
            }
            if (c == CharT('1'))
            {
                val = true;
                ++beg;
                return beg;
            }

            long long temp = 0;
            return get(beg, end, io, err, temp);
        }

        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, long &val) const
        {
            return get_impl(beg, end, io, err, val);
        }
        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, unsigned long &val) const
        {
            return get_impl(beg, end, io, err, val);
        }
        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, long long &val) const
        {
            return get_impl(beg, end, io, err, val);
        }
        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, unsigned long long &val) const
        {
            return get_impl(beg, end, io, err, val);
        }
        iter_type get(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, double &val) const
        {
            return get_impl(beg, end, io, err, val);
        }

    private:
        template<typename T>
        iter_type get_impl(iter_type beg, iter_type end, ios_base &io, ios_base::iostate &err, T &val) const
        {
            return get<T>(beg, end, io, err, val);
        }
    };

    template<typename CharT, typename OutputIt = ostreambuf_iterator<CharT>>
    class num_put
    {
    public:
        using char_type = CharT;
        using iter_type = OutputIt;

        explicit num_put(size_t /*refs*/ = 0) noexcept {}

        template<typename ValType>
        iter_type put(iter_type out, ios_base &io, char_type /*fill*/, ValType val) const
        {
            char buf[128];
            int len = format_value(val, buf, sizeof(buf), io);
            for (int i = 0; i < len; ++i)
            {
                *out = static_cast<CharT>(buf[i]);
                ++out;
            }
            return out;
        }

        iter_type put(iter_type out, ios_base &io, char_type fill, bool val) const
        {
            if (static_cast<bool>(io.flags() & ios_base::boolalpha))
            {
                const char *s = val ? "true" : "false";
                while (*s)
                {
                    *out = static_cast<CharT>(*s++);
                    ++out;
                }
                return out;
            }
            long lval = val ? 1 : 0;
            return put(out, io, fill, lval);
        }

        iter_type put(iter_type out, ios_base &io, char_type fill, long val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, unsigned long val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, long long val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, unsigned long long val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, double val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, long double val) const
        {
            return put_impl(out, io, fill, val);
        }
        iter_type put(iter_type out, ios_base &io, char_type fill, const void *val) const
        {
            return put_impl(out, io, fill, val);
        }

    private:
        template<typename T>
        iter_type put_impl(iter_type out, ios_base &io, char_type fill, T val) const
        {
            return put<T>(out, io, fill, val);
        }

        static int format_digits(unsigned long long mag, char *buf, size_type max_len, int base, bool uppercase)
        {
            if (max_len == 0)
                return 0;

            char tmp[64];
            int n = 0;

            if (mag == 0)
            {
                tmp[n++] = '0';
            } else
            {
                while (mag != 0 && n < static_cast<int>(sizeof(tmp)))
                {
                    int digit = static_cast<int>(mag % static_cast<unsigned long long>(base));
                    tmp[n++]  = digit_to_char(digit, uppercase);
                    mag /= static_cast<unsigned long long>(base);
                }
            }

            int written = (n < static_cast<int>(max_len)) ? n : static_cast<int>(max_len);
            // tmp was built least-significant-digit first; reverse into buf.
            for (int i = 0; i < written; ++i)
                buf[i] = tmp[n - 1 - i];
            return written;
        }

        template<typename T>
        int format_integral(T val, char *buf, size_type max_len, const ios_base &io) const
        {
            const ios_base::fmtflags flags = io.flags();
            const int base                 = ((flags & ios_base::basefield) == ios_base::hex)   ? 16
                                             : ((flags & ios_base::basefield) == ios_base::oct) ? 8
                                                                                                : 10;
            const bool uppercase           = static_cast<bool>(flags & ios_base::uppercase);
            const bool show_base           = static_cast<bool>(flags & ios_base::showbase);
            const bool show_pos            = static_cast<bool>(flags & ios_base::showpos);

            size_type pos = 0;
            unsigned long long mag;
            bool negative = false;

            if constexpr (is_signed_v<T>)
            {
                negative = val < 0;
                // Avoid UB negating the minimum value: widen through unsigned.
                mag = negative ? (static_cast<unsigned long long>(0) - static_cast<unsigned long long>(val))
                               : static_cast<unsigned long long>(val);
            } else
            {
                mag = static_cast<unsigned long long>(val);
            }

            if (negative && pos < max_len)
                buf[pos++] = '-';
            else if (show_pos && base == 10 && pos < max_len)
                buf[pos++] = '+';

            if (show_base && mag != 0)
            {
                if (base == 16 && pos + 1 < max_len)
                {
                    buf[pos++] = '0';
                    buf[pos++] = uppercase ? 'X' : 'x';
                } else if (base == 8 && pos < max_len)
                {
                    buf[pos++] = '0';
                }
            }

            pos += static_cast<size_type>(format_digits(mag, buf + pos, max_len - pos, base, uppercase));
            return static_cast<int>(pos);
        }

        template<typename T>
        int format_floating(T val, char *buf, size_type max_len, const ios_base &io) const
        {
            const ios_base::fmtflags flags = io.flags();
            const bool scientific          = (flags & ios_base::floatfield) == ios_base::scientific;
            streamsize prec                = io.precision();
            if (prec < 0)
                prec = 6;

            auto d        = static_cast<double>(val);
            size_type pos = 0;

            if (d < 0.0 || (d == 0.0 && __builtin_signbit(d)))
            {
                if (pos < max_len)
                    buf[pos++] = '-';
                d = -d;
            } else if (static_cast<bool>(flags & ios_base::showpos))
            {
                if (pos < max_len)
                    buf[pos++] = '+';
            }

            int exp = 0;
            if (scientific && d != 0.0)
            {
                while (d >= 10.0)
                {
                    d /= 10.0;
                    ++exp;
                }
                while (d < 1.0)
                {
                    d *= 10.0;
                    --exp;
                }
            }

            double scale = 1.0;
            for (streamsize i = 0; i < prec; ++i)
                scale *= 10.0;
            d = static_cast<double>(static_cast<unsigned long long>(d * scale + 0.5)) / scale;

            auto whole = static_cast<unsigned long long>(d);
            pos += static_cast<size_type>(format_digits(whole, buf + pos, max_len - pos, 10, false));

            if (prec > 0)
            {
                if (pos < max_len)
                    buf[pos++] = '.';
                double frac = (d - static_cast<double>(whole)) * scale;
                auto frac_i = static_cast<unsigned long long>(frac + 0.5);

                // Zero-pad the fractional part out to `prec` digits.
                char frac_buf[32];
                int frac_len = format_digits(frac_i, frac_buf, sizeof(frac_buf), 10, false);
                for (int i = frac_len; i < static_cast<int>(prec) && pos < max_len; ++i)
                    buf[pos++] = '0';
                for (int i = 0; i < frac_len && pos < max_len; ++i)
                    buf[pos++] = frac_buf[i];
            }

            if (scientific)
            {
                if (pos < max_len)
                    buf[pos++] = static_cast<bool>(flags & ios_base::uppercase) ? 'E' : 'e';
                if (pos < max_len)
                    buf[pos++] = (exp < 0) ? '-' : '+';
                auto aexp = static_cast<unsigned long long>(exp < 0 ? -exp : exp);
                char exp_buf[8];
                int exp_len = format_digits(aexp, exp_buf, sizeof(exp_buf), 10, false);
                if (exp_len < 2 && pos < max_len)
                    buf[pos++] = '0'; // at least 2 exponent digits
                for (int i = 0; i < exp_len && pos < max_len; ++i)
                    buf[pos++] = exp_buf[i];
            }

            return static_cast<int>(pos);
        }

        template<typename T>
        int format_value(T val, char *buf, size_type max_len, const ios_base &io) const
        {
            if constexpr (is_pointer_v<T> || is_same_v<T, const void *>)
            {
                const auto addr = reinterpret_cast<unsigned long long>(val);
                size_type pos   = 0;
                if (pos + 1 < max_len)
                {
                    buf[pos++] = '0';
                    buf[pos++] = 'x';
                }
                pos += static_cast<size_type>(format_digits(addr, buf + pos, max_len - pos, 16, false));
                return static_cast<int>(pos);
            } else if constexpr (is_floating_point_v<T>)
            {
                return format_floating(val, buf, max_len, io);
            } else
            {
                return format_integral(val, buf, max_len, io);
            }
        }
    };
} // namespace SFTL
