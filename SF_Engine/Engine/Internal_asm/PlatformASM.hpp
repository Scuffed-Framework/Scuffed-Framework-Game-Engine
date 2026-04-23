#pragma once

#include <cstdint>
#include <cmath>
#include <bit>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <xmmintrin.h>  // SSE
    #include <emmintrin.h>  // SSE2
    #include <pmmintrin.h>  // SSE3
#endif

namespace SF::Engine::ASM {

// ============================================================================
// Fast Inverse Square Root
// ============================================================================
inline float FastInvSqrt(float x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang with inline assembly
    float y;
    asm volatile(
        "rsqrtss %1, %0"
        : "=x"(y)
        : "x"(x));
    return y;
    
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    // MSVC using intrinsics
    __m128 in = _mm_set_ss(x);
    __m128 out = _mm_rsqrt_ss(in);
    return _mm_cvtss_f32(out);
    
#else
    // Fallback: Quake III fast inverse square root
    union {
        float f;
        uint32_t i;
    } conv = { x };
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5f - (x * 0.5f * conv.f * conv.f);
    return conv.f;
#endif
}

// ============================================================================
// Fast Sine
// ============================================================================
inline float FastSin(float x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang with inline assembly (x87 FPU)
    float result;
    asm volatile(
        "flds %1\n\t"
        "fsin\n\t"
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    return result;
    
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    // MSVC: x87 not available in x64 mode, use standard library
    // In 32-bit mode, could use inline asm but MSVC x64 doesn't support it
    return std::sin(x);
    
#else
    // Fallback
    return std::sin(x);
#endif
}

// ============================================================================
// Count Leading Zeros
// ============================================================================
inline int CountLeadingZeros(uint32_t x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    #if defined(__LZCNT__)
        // Use inline assembly if LZCNT is available
        int result;
        asm volatile(
            "lzcnt %1, %0"
            : "=r"(result)
            : "r"(x));
        return result;
    #else
        // Use compiler builtin
        return x == 0 ? 32 : __builtin_clz(x);
    #endif
    
#elif defined(_MSC_VER)
    // MSVC intrinsic
    #if defined(__AVX2__) || defined(__LZCNT__)
        return (int)__lzcnt(x);
    #else
        unsigned long index;
        return _BitScanReverse(&index, x) ? (31 - index) : 32;
    #endif
    
#else
    // Fallback using C++20 std::countl_zero if available
    #if __cpp_lib_bitops >= 201907L
        return std::countl_zero(x);
    #else
        // Manual fallback
        if (x == 0) return 32;
        int count = 0;
        while ((x & 0x80000000) == 0) {
            x <<= 1;
            ++count;
        }
        return count;
    #endif
#endif
}

// ============================================================================
// Horizontal Add (SSE3)
// ============================================================================
inline float HorizontalAdd(__m128 v) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang with inline assembly
    float result;
    asm volatile(
        "haddps %1, %1\n\t"
        "haddps %1, %1\n\t"
        "movss %1, %0"
        : "=m"(result)
        : "x"(v));
    return result;
    
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    // MSVC using intrinsics
    __m128 temp = _mm_hadd_ps(v, v);
    temp = _mm_hadd_ps(temp, temp);
    return _mm_cvtss_f32(temp);
    
#else
    // Fallback: manual addition
    alignas(16) float arr[4];
    _mm_store_ps(arr, v);
    return arr[0] + arr[1] + arr[2] + arr[3];
#endif
}

// ============================================================================
// Platform Detection Macros (for conditional compilation elsewhere)
// ============================================================================
#if defined(__GNUC__) || defined(__clang__)
    #define SF_ASM_GNU_STYLE 1
#elif defined(_MSC_VER)
    #define SF_ASM_MSVC_STYLE 1
#endif

#if defined(__SSE__)
    #define SF_HAS_SSE 1
#endif

#if defined(__SSE3__)
    #define SF_HAS_SSE3 1
#endif

#if defined(__LZCNT__) || defined(__AVX2__)
    #define SF_HAS_LZCNT 1
#endif

} // namespace SF::Engine::ASM