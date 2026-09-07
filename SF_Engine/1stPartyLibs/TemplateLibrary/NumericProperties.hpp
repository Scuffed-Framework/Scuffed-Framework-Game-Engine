#pragma once
#include "TypeTraits.hpp"

namespace SFTL
{
    // Standard-style numeric_limits primary template
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

    // Macro for integer types
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
} // namespace SFTL
