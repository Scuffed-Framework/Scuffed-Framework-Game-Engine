/******************************************************************************/
/* Float.hpp                                                                  */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
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

// stdfloat header implementation (because msvc doesn't support it yet)
#pragma once

#include "../Operations.hpp"

namespace SFTL
{
    /*
    as in cppreference, the following types are defined in <stdfloat> header
    Types       Literal suffix   Predefined macro       C language type   bits of storage   bits of precision   bits of exponent   max exponent
    float16_t	f16 or F16	     __STDCPP_FLOAT16_T__	_Float16	      16	            11	              5	                  15
    float32_t	f32 or F32	     __STDCPP_FLOAT32_T__	_Float32	      32	            24	              8                   27
    float64_t	f64 or F64	     __STDCPP_FLOAT64_T__	_Float64	      64	            53	              11	              1023
    float128_t	f128 or F128	 __STDCPP_FLOAT128_T__	_Float128	      128	            113	              15	              16383
    bfloat16_t	bf16 or BF16	 __STDCPP_BFLOAT16_T__	(N/A)	          16	            8	              8	                  127
    */

#if defined(__STDCPP_FLOAT16_T__)

    using float16_t = decltype(0.0f16);

#elif defined(__FLT16_MANT_DIG__) && !defined(_MSC_VER)

    using float16_t = _Float16;
#define __STDCPP_FLOAT16_T__ 1

#else

    // Portable IEEE-754 binary16 storage type (no native HW support assumed).
    // Not a drop-in arithmetic type, convert to float to do math, convert
    // back to store. Good enough for asset/serialization use in an engine.
    class float16_t
    {
    public:
        float16_t() noexcept : bits_(0) {}
        float16_t(float f) noexcept : bits_(from_float(f)) {}

        operator float() const noexcept { return to_float(bits_); }

        static float16_t from_bits(unsigned short b) noexcept
        {
            float16_t r;
            r.bits_ = b;
            return r;
        }
        unsigned short to_bits() const noexcept { return bits_; }

    private:
        unsigned short bits_;

        static unsigned short from_float(float f) noexcept
        {
            unsigned int x;
            static_assert(sizeof(x) == sizeof(f), "expected 32-bit float");
            SFTL_BI_MCPY(&x, &f, sizeof(x));

            unsigned int sign = (x >> 16) & 0x8000u;
            int exp = int((x >> 23) & 0xFF) - 127 + 15;
            unsigned int mant = x & 0x7FFFFFu;

            if (exp <= 0)
            {
                // Too small: flush to signed zero (subnormals not handled).
                return static_cast<unsigned short>(sign);
            }
            if (exp >= 0x1F)
            {
                // Overflow/inf/nan -> inf, preserve sign.
                return static_cast<unsigned short>(sign | 0x7C00u);
            }

            return static_cast<unsigned short>(sign | (unsigned int(exp) << 10) | (mant >> 13));
        }

        static float to_float(unsigned short h) noexcept
        {
            unsigned int sign = (h & 0x8000u) << 16;
            unsigned int exp = (h >> 10) & 0x1Fu;
            unsigned int mant = h & 0x3FFu;
            unsigned int bits;

            if (exp == 0)
            {
                bits = sign; // zero / subnormal-as-zero
            }
            else if (exp == 0x1F)
            {
                bits = sign | 0x7F800000u | (mant << 13); // inf/nan
            }
            else
            {
                bits = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
            }

            float f;
            SFTL_BI_MCPY(&f, &bits, sizeof(f));
            return f;
        }
    };
#define __STDCPP_FLOAT16_T__ 1

#endif

#if defined(__STDCPP_FLOAT32_T__)

    using float32_t = decltype(0.0f32);

#else

    using float32_t = float;
#define __STDCPP_FLOAT32_T__ 1

#endif

#if defined(__STDCPP_FLOAT64_T__)

    using float64_t = decltype(0.0f64);

#else

    using float64_t = double;
#define __STDCPP_FLOAT64_T__ 1

#endif

#if defined(__STDCPP_FLOAT128_T__)

    using float128_t = decltype(0.0f128);

#elif defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)

    using float128_t = __float128;
#define __STDCPP_FLOAT128_T__ 1

#else

    // MSVC/x64 has no true 128-bit float. `long double` on MSVC is just a
    // 64-bit double in disguise, so this is NOT actually quad precision --
    // it only exists so the typedef doesn't disappear and break builds.
    // Do not rely on this for real 128-bit precision under MSVC.
    using float128_t = long double;
#define __STDCPP_FLOAT128_T__ 1

#endif

#if defined(__STDCPP_BFLOAT16_T__)

    using bfloat16_t = decltype(0.0bf16);

#else

    // bfloat16 is just the top 16 bits of an IEEE-754 float32 (same exponent
    // range as float32, reduced mantissa) so conversion is trivial truncation.
    class bfloat16_t
    {
    public:
        bfloat16_t() noexcept : bits_(0) {}
        bfloat16_t(float f) noexcept : bits_(from_float(f)) {}

        operator float() const noexcept { return to_float(bits_); }

        static bfloat16_t from_bits(unsigned short b) noexcept
        {
            bfloat16_t r;
            r.bits_ = b;
            return r;
        }
        unsigned short to_bits() const noexcept { return bits_; }

    private:
        unsigned short bits_;

        static unsigned short from_float(float f) noexcept
        {
            unsigned int x;
            SFTL_BI_MCPY(&x, &f, sizeof(x));
            // round-to-nearest-even on the truncated 16 bits
            unsigned int rounded = x + 0x7FFFu + ((x >> 16) & 1u);
            return static_cast<unsigned short>(rounded >> 16);
        }

        static float to_float(unsigned short b) noexcept
        {
            unsigned int bits = static_cast<unsigned int>(b) << 16;
            float f;
            SFTL_BI_MCPY(&f, &bits, sizeof(f));
            return f;
        }
    };
#define __STDCPP_BFLOAT16_T__ 1

#endif

} // namespace SFTL