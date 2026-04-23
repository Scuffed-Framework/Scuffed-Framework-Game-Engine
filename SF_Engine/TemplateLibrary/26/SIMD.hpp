#ifndef BETTER_SIMD_HPP
#define BETTER_SIMD_HPP

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <iostream>

namespace SFTL {

    // --------------------------------------------------------------------------
    // Concepts
    // --------------------------------------------------------------------------
    
    template <typename T>
    concept Vectorizable = std::is_arithmetic_v<T>;

    // --------------------------------------------------------------------------
    // ABI Tags (Simplified for this implementation)
    // --------------------------------------------------------------------------
    
    // In a real library, these would select AVX/SSE/NEON impls. 
    // Here, we use a fixed size that aligns well with registers.
    template <int N>
    struct fixed_size { static constexpr int size = N; };

    // Auto-detect best width (simplified heuristic: 128-bit chunks)
    template <typename T>
    using native = fixed_size<16 / sizeof(T)>;

    // --------------------------------------------------------------------------
    // The SIMD Class
    // --------------------------------------------------------------------------

    template <Vectorizable T, typename Abi = native<T>>
    class simd {
    public:
        static constexpr size_t size = Abi::size;
        using value_type = T;
        using abi_type = Abi;

        // Alignment helps MSVC auto-vectorizer use aligned loads/stores
        // Align to 32 bytes (AVX2 friendly) or 64 bytes (AVX512 friendly)
        alignas(32) std::array<T, size> _data;

        // --- Constructors ---

        // Default: zero init
        constexpr simd() : _data{} {}

        // Broadcast constructor: simd<float>(1.5f) -> [1.5, 1.5, ...]
        constexpr simd(T val) {
            _data.fill(val);
        }

        // Array constructor
        constexpr simd(const std::array<T, size>& arr) : _data(arr) {}

        // Load from pointer
        static simd load(const T* ptr) {
            simd res;
            // MSVC auto-vectorizes this `copy_n` extremely well with /O2
            std::copy_n(ptr, size, res._data.begin());
            return res;
        }

        // Store to pointer
        void store(T* ptr) const {
            std::copy_n(_data.begin(), size, ptr);
        }

        // --- Element Access ---
        
        constexpr T& operator[](size_t i) { return _data[i]; }
        constexpr const T& operator[](size_t i) const { return _data[i]; }

        // --- arithmetic operators (Friend functions for symmetry) ---

        // We use loops here. Trust the compiler. 
        // MSVC generates 'vaddps', 'vmulps' etc. automatically for these loops.

        friend simd operator+(const simd& lhs, const simd& rhs) {
            simd res;
            for (size_t i = 0; i < size; ++i) res[i] = lhs[i] + rhs[i];
            return res;
        }

        friend simd operator-(const simd& lhs, const simd& rhs) {
            simd res;
            for (size_t i = 0; i < size; ++i) res[i] = lhs[i] - rhs[i];
            return res;
        }

        friend simd operator*(const simd& lhs, const simd& rhs) {
            simd res;
            for (size_t i = 0; i < size; ++i) res[i] = lhs[i] * rhs[i];
            return res;
        }

        friend simd operator/(const simd& lhs, const simd& rhs) {
            simd res;
            for (size_t i = 0; i < size; ++i) res[i] = lhs[i] / rhs[i];
            return res;
        }

        // --- Compound Assignment ---
        
        simd& operator+=(const simd& rhs) { *this = *this + rhs; return *this; }
        simd& operator-=(const simd& rhs) { *this = *this - rhs; return *this; }
        simd& operator*=(const simd& rhs) { *this = *this * rhs; return *this; }
        simd& operator/=(const simd& rhs) { *this = *this / rhs; return *this; }
    };

    // --------------------------------------------------------------------------
    // Math Functions (Element-wise)
    // --------------------------------------------------------------------------

    template <Vectorizable T, typename Abi>
    simd<T, Abi> sqrt(const simd<T, Abi>& v) {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i) {
            res[i] = std::sqrt(v[i]);
        }
        return res;
    }

    template <Vectorizable T, typename Abi>
    simd<T, Abi> min(const simd<T, Abi>& a, const simd<T, Abi>& b) {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i) {
            res[i] = std::min(a[i], b[i]);
        }
        return res;
    }
    
    template <Vectorizable T, typename Abi>
    simd<T, Abi> max(const simd<T, Abi>& a, const simd<T, Abi>& b) {
        simd<T, Abi> res;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i) {
            res[i] = std::max(a[i], b[i]);
        }
        return res;
    }

    // --------------------------------------------------------------------------
    // Reductions
    // --------------------------------------------------------------------------

    template <Vectorizable T, typename Abi>
    T reduce_add(const simd<T, Abi>& v) {
        T sum = 0;
        for (size_t i = 0; i < simd<T, Abi>::size; ++i) sum += v[i];
        return sum;
    }
}

#endif // BETTER_SIMD_HPP