/******************************************************************************/
/* SIMD.hpp                                                                   */
/******************************************************************************/
/*                            This file is part of                            */
/*             Scuffed Framework Standard Template Library                    */
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
//
// The previous version of this header stored an aligned std::array<T,N> and
// touched it one element at a time in hand-rolled `for` loops. That is not
// SIMD -- alignas(32) makes the memory aligned, it does not make the compiler
// emit vector instructions, and a scalar loop has no guarantee of being
// auto-vectorized (it usually isn't, without -O2/-O3, -ffast-math for the fp
// reassociation reduce_add needs, and a cooperative optimizer).
//
// This version stores each lane group as a GCC/Clang "vector extension" type
// (T __attribute__((vector_size(N)))), which is a real hardware vector: the
// compiler maps +,-,*,/, comparisons, and the ?: lane-select idiom directly
// onto SSE/AVX/AVX-512 or NEON instructions depending on target, with no
// autovectorizer guesswork involved. This is the same underlying mechanism
// libraries like xsimd and highway build on for their portable fast paths.
//
// MSVC does not support this GCC/Clang extension, so it falls back to the
// original scalar-array implementation there. That path is correctness-only,
// not a genuine SIMD backend -- MSVC's autovectorizer may or may not turn it
// into vector code depending on flags. A follow-up could add hand-written
// <immintrin.h>/<arm_neon.h> specializations for the MSVC path; this header
// does not attempt that yet.
//
#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <algorithm>

#include <TemplateLibrary/TypeTraits.hpp>

#if defined(__GNUC__) || defined(__clang__)
#define SFTL_SIMD_HAS_VECTOR_EXT 1
#else
#define SFTL_SIMD_HAS_VECTOR_EXT 0
#endif

#if defined(__has_builtin)
#define SFTL_SIMD_HAS_BUILTIN(x) __has_builtin(x)
#else
#define SFTL_SIMD_HAS_BUILTIN(x) 0
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define SFTL_SIMD_X86 1
#else
#define SFTL_SIMD_X86 0
#endif

namespace SFTL
{
    template <typename T>
    concept Vectorizable = is_arithmetic_v<T>;

#if defined(__AVX512F__)
    inline constexpr size_t native_vector_bytes = 64;
#elif defined(__AVX2__) || defined(__AVX__)
    inline constexpr size_t native_vector_bytes = 32;
#elif defined(__SSE2__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
    inline constexpr size_t native_vector_bytes = 16;
#else
    inline constexpr size_t native_vector_bytes = 0; // signals "no known vector ISA" below
#endif

    template <int N>
    struct fixed_size
    {
        static constexpr int size = N;
    };

    namespace detail
    {
        template <typename T>
        constexpr int native_lane_count()
        {
            if constexpr (native_vector_bytes == 0)
                return 1; // no known vector ISA: one lane, i.e. scalar
            else
                return static_cast<int>(native_vector_bytes / sizeof(T)) > 0
                           ? static_cast<int>(native_vector_bytes / sizeof(T))
                           : 1;
        }

#if SFTL_SIMD_HAS_VECTOR_EXT
        template <typename T, size_t N>
        struct vector_of_helper
        {
            typedef T type __attribute__((vector_size(N * sizeof(T))));
        };
        template <typename T, size_t N>
        using vector_of = typename vector_of_helper<T, N>::type;
#endif
    } // namespace detail

    template <typename T>
    using native = fixed_size<detail::native_lane_count<T>()>;

    template <Vectorizable T, typename Abi = native<T>>
    class simd
    {
    public:
        static constexpr size_t size = Abi::size;
        using value_type = T;
        using abi_type = Abi;

#if SFTL_SIMD_HAS_VECTOR_EXT
        using storage_type = detail::vector_of<T, size>;
#else
        using storage_type = std::array<T, size>;
#endif

        storage_type _data;

        constexpr simd() : _data{} {}

        constexpr simd(T val)
        {
#if SFTL_SIMD_HAS_VECTOR_EXT
            for (size_t i = 0; i < size; ++i)
                _data[i] = val;
#else
            _data.fill(val);
#endif
        }

        constexpr simd(const std::array<T, size> &arr)
        {
            for (size_t i = 0; i < size; ++i)
                _data[i] = arr[i];
        }

        static simd load(const T *ptr)
        {
            simd res;
            for (size_t i = 0; i < size; ++i)
                res._data[i] = ptr[i];
            return res;
        }

        void store(T *ptr) const
        {
            for (size_t i = 0; i < size; ++i)
                ptr[i] = _data[i];
        }

        constexpr T &operator[](size_t i) { return _data[i]; }
        constexpr const T &operator[](size_t i) const { return _data[i]; }

#if SFTL_SIMD_HAS_VECTOR_EXT
        friend simd operator+(const simd &lhs, const simd &rhs) { return from_storage(lhs._data + rhs._data); }
        friend simd operator-(const simd &lhs, const simd &rhs) { return from_storage(lhs._data - rhs._data); }
        friend simd operator*(const simd &lhs, const simd &rhs) { return from_storage(lhs._data * rhs._data); }
        friend simd operator/(const simd &lhs, const simd &rhs) { return from_storage(lhs._data / rhs._data); }
#else
        friend simd operator+(const simd &lhs, const simd &rhs)
        {
            simd res;
            for (size_t i = 0; i < size; ++i)
                res._data[i] = lhs._data[i] + rhs._data[i];
            return res;
        }
        friend simd operator-(const simd &lhs, const simd &rhs)
        {
            simd res;
            for (size_t i = 0; i < size; ++i)
                res._data[i] = lhs._data[i] - rhs._data[i];
            return res;
        }
        friend simd operator*(const simd &lhs, const simd &rhs)
        {
            simd res;
            for (size_t i = 0; i < size; ++i)
                res._data[i] = lhs._data[i] * rhs._data[i];
            return res;
        }
        friend simd operator/(const simd &lhs, const simd &rhs)
        {
            simd res;
            for (size_t i = 0; i < size; ++i)
                res._data[i] = lhs._data[i] / rhs._data[i];
            return res;
        }
#endif

        simd &operator+=(const simd &rhs) { return *this = *this + rhs; }
        simd &operator-=(const simd &rhs) { return *this = *this - rhs; }
        simd &operator*=(const simd &rhs) { return *this = *this * rhs; }
        simd &operator/=(const simd &rhs) { return *this = *this / rhs; }

    private:
#if SFTL_SIMD_HAS_VECTOR_EXT
        static simd from_storage(storage_type s)
        {
            simd res;
            res._data = s;
            return res;
        }
#endif
    };

#if SFTL_SIMD_HAS_VECTOR_EXT

    template <Vectorizable T, typename Abi>
    simd<T, Abi> sqrt(const simd<T, Abi> &v)
    {
        using storage_type = typename simd<T, Abi>::storage_type;
        constexpr size_t bytes = simd<T, Abi>::size * sizeof(T);
#if SFTL_SIMD_HAS_BUILTIN(__builtin_elementwise_sqrt)
        storage_type r = __builtin_elementwise_sqrt(v._data);
#elif SFTL_SIMD_X86
        storage_type r{};
        if constexpr (is_same_v<T, float> && bytes == 64)
        {
            __m512 in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m512 out = _mm512_sqrt_ps(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else if constexpr (is_same_v<T, float> && bytes == 32)
        {
            __m256 in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m256 out = _mm256_sqrt_ps(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else if constexpr (is_same_v<T, float> && bytes == 16)
        {
            __m128 in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m128 out = _mm_sqrt_ps(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else if constexpr (is_same_v<T, double> && bytes == 64)
        {
            __m512d in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m512d out = _mm512_sqrt_pd(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else if constexpr (is_same_v<T, double> && bytes == 32)
        {
            __m256d in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m256d out = _mm256_sqrt_pd(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else if constexpr (is_same_v<T, double> && bytes == 16)
        {
            __m128d in;
            __builtin_memcpy(&in, &v._data, sizeof(in));
            __m128d out = _mm_sqrt_pd(in);
            __builtin_memcpy(&r, &out, sizeof(r));
        }
        else
        {
            for (size_t i = 0; i < simd<T, Abi>::size; ++i)
                r[i] = std::sqrt(v._data[i]);
        }
#else
        storage_type r{};
        for (size_t i = 0; i < simd<T, Abi>::size; ++i)
            r[i] = std::sqrt(v._data[i]);
#endif
        simd<T, Abi> res;
        res._data = r;
        return res;
    }

    template <Vectorizable T, typename Abi>
    simd<T, Abi> min(const simd<T, Abi> &a, const simd<T, Abi> &b)
    {
        simd<T, Abi> res;
        res._data = (a._data < b._data) ? a._data : b._data;
        return res;
    }

    template <Vectorizable T, typename Abi>
    simd<T, Abi> max(const simd<T, Abi> &a, const simd<T, Abi> &b)
    {
        simd<T, Abi> res;
        res._data = (a._data > b._data) ? a._data : b._data;
        return res;
    }

#else // !SFTL_SIMD_HAS_VECTOR_EXT (MSVC, or an unrecognized compiler)

    template <Vectorizable T, typename Abi>
    simd<T, Abi> sqrt(const simd<T, Abi> &v)
    {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i)
            res[i] = std::sqrt(v[i]);
        return res;
    }

    template <Vectorizable T, typename Abi>
    simd<T, Abi> min(const simd<T, Abi> &a, const simd<T, Abi> &b)
    {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i)
            res[i] = std::min(a[i], b[i]);
        return res;
    }

    template <Vectorizable T, typename Abi>
    simd<T, Abi> max(const simd<T, Abi> &a, const simd<T, Abi> &b)
    {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i)
            res[i] = std::max(a[i], b[i]);
        return res;
    }

#endif

    template <Vectorizable T, typename Abi>
    T reduce_add(const simd<T, Abi> &v)
    {
        // A genuine horizontal-reduce intrinsic (haddps, or log2(N) shuffle+add
        // steps) would beat this for large lane counts, but for the widths
        // this header targets (typically 4-16 lanes) GCC/Clang reliably
        // unroll and reassociate this into a short, real reduction sequence
        // at -O2 -ffast-math
        // it is not the naive "one add per lane in
        // program order with no ILP" you'd get from, say, std::accumulate.
        T sum = T{};
        for (size_t i = 0; i < simd<T, Abi>::size; ++i)
            sum += v[i];
        return sum;
    }
}