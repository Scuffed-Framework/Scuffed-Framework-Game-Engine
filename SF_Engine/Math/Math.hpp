/******************************************************************************/
/* Math.hpp                                                                   */
/******************************************************************************/
/* This file is part of                            */
/* SF Game Engine                               */
/******************************************************************************/
/* MIT License                                                                */
/* */
/* Copyright (c) 2025-present Martin.                                         */
/* */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/* */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/* */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/* */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <random>

// Required for std::hash<glm::vec...> specializations
#include <glm/gtx/hash.hpp>

namespace SF::Engine
{
    /**
     * @brief Concept for arithmetic types
     */
    template <typename T>
    concept Arithmetic = std::is_arithmetic_v<T>;

    /**
     * @brief Concept for floating point types
     */
    template <typename T>
    concept FloatingPoint = std::is_floating_point_v<T>;

    /**
     * @brief Class that holds various mathematical functions and constants
     */
    class Maths
    {
    public:
        // Use C++20 mathematical constants (more precise)
        template <FloatingPoint T = float>
        static constexpr T PI = std::numbers::pi_v<T>;

        template <FloatingPoint T = float>
        static constexpr T TAU = std::numbers::pi_v<T> * T(2); // 2π

        template <FloatingPoint T = float>
        static constexpr T E = std::numbers::e_v<T>;

        template <FloatingPoint T = float>
        static constexpr T GOLDEN_RATIO =
            T(1.618033988749894848204586834365638117720309179805762862135);

        template <FloatingPoint T = float>
        static constexpr T SQRT2 = std::numbers::sqrt2_v<T>;

        template <FloatingPoint T = float>
        static constexpr T SQRT3 = std::numbers::sqrt3_v<T>;

        // Default epsilon for floating point comparisons
        template <FloatingPoint T = float>
        static constexpr T EPSILON = std::numeric_limits<T>::epsilon();

        Maths() = delete;

        /**
         * @brief Get thread-local random number generator
         */
        static std::mt19937 &GetRNG()
        {
            thread_local std::mt19937 rng(std::random_device{}());
            return rng;
        }

        /**
         * @brief Generates a random value from between a range
         * @param min The min value
         * @param max The max value
         * @return The randomly selected value within the range
         */
        template <FloatingPoint T = float>
        static T Random(T min = T(0), T max = T(1))
        {
            std::uniform_real_distribution<T> dist(min, max);
            return dist(GetRNG());
        }

        /**
         * @brief Generates a random integer value from between a range
         * @param min The min value (inclusive)
         * @param max The max value (inclusive)
         * @return The randomly selected value within the range
         */
        template <std::integral T>
        static T RandomInt(T min, T max)
        {
            std::uniform_int_distribution<T> dist(min, max);
            return dist(GetRNG());
        }

        /**
         * @brief Generates a value from a normal distribution using Box-Muller
         * @param mean The mean of the distribution
         * @param standardDeviation The standard deviation of the distribution
         * @return A normally distributed value
         */
        template <FloatingPoint T = float>
        static T RandomNormal(T mean = T(0), T standardDeviation = T(1))
        {
            std::normal_distribution<T> dist(mean, standardDeviation);
            return dist(GetRNG());
        }

        /**
         * @brief Creates a number between two numbers, logarithmic distribution
         * @param min The min value
         * @param max The max value
         * @return The final random number
         */
        template <FloatingPoint T = float>
        static T RandomLog(T min, T max)
        {
            auto logMin = std::log(min);
            auto logMax = std::log(max);
            auto scale = Random<T>();
            return std::exp(logMin + scale * (logMax - logMin));
        }

        /**
         * @brief Converts degrees to radians
         * @param degrees The degrees value
         * @return The radians value
         */
        template <Arithmetic T>
        static constexpr auto Radians(T degrees) noexcept
        {
            using Result = std::conditional_t<std::is_floating_point_v<T>, T, double>;
            return static_cast<Result>(degrees) * PI<Result> / Result(180);
        }

        /**
         * @brief Converts radians to degrees
         * @param radians The radians value
         * @return The degrees value
         */
        template <Arithmetic T>
        static constexpr auto Degrees(T radians) noexcept
        {
            using Result = std::conditional_t<std::is_floating_point_v<T>, T, double>;
            return static_cast<Result>(radians) * Result(180) / PI<Result>;
        }

        /**
         * @brief Normalizes an angle into the range of 0-360
         * @param degrees The source angle
         * @return The normalized angle
         */
        template <FloatingPoint T = float>
        static T WrapDegrees(T degrees) noexcept
        {
            degrees = std::fmod(degrees, T(360));
            if (degrees < T(0))
                degrees += T(360);
            return degrees;
        }

        /**
         * @brief Normalizes an angle into the range of -180 to 180
         * @param degrees The source angle
         * @return The normalized angle
         */
        template <FloatingPoint T = float>
        static T WrapDegreesSigned(T degrees) noexcept
        {
            degrees = WrapDegrees(degrees);
            if (degrees > T(180))
                degrees -= T(360);
            return degrees;
        }

        /**
         * @brief Normalizes an angle into the range of 0-2π
         * @param radians The source angle
         * @return The normalized angle
         */
        template <FloatingPoint T = float>
        static T WrapRadians(T radians) noexcept
        {
            radians = std::fmod(radians, TAU<T>);
            if (radians < T(0))
                radians += TAU<T>;
            return radians;
        }

        /**
         * @brief Normalizes an angle into the range of -π to π
         * @param radians The source angle
         * @return The normalized angle
         */
        template <FloatingPoint T = float>
        static T WrapRadiansSigned(T radians) noexcept
        {
            radians = WrapRadians(radians);
            if (radians > PI<T>)
                radians -= TAU<T>;
            return radians;
        }

        /**
         * @brief Rounds a value to a number of decimal places
         * @param value The value to round
         * @param places Number of decimal places
         * @return The rounded value
         */
        template <FloatingPoint T = float>
        static T RoundToPlaces(T value, int32_t places)
        {
            T multiplier = std::pow(T(10), places);
            return std::round(value * multiplier) / multiplier;
        }

        /**
         * @brief Floors the value if less than the minimum threshold
         * @param threshold The minimum threshold
         * @param value The value
         * @return Value with deadband applied
         */
        template <Arithmetic T>
        static constexpr T Deadband(T threshold, T value) noexcept
        {
            return std::abs(value) >= std::abs(threshold) ? value : T(0);
        }

        /**
         * @brief Checks if two values are almost equal within epsilon
         * @param a The first value
         * @param b The second value
         * @param epsilon The tolerance (default uses type's epsilon)
         * @return True if values are almost equal
         */
        template <FloatingPoint T>
        static constexpr bool AlmostEqual(T a, T b, T epsilon = EPSILON<T>) noexcept
        {
            return std::abs(a - b) <= epsilon * std::max({T(1), std::abs(a), std::abs(b)});
        }

        /**
         * @brief Checks if a value is approximately zero
         * @param value The value to check
         * @param epsilon The tolerance
         * @return True if value is approximately zero
         */
        template <FloatingPoint T>
        static constexpr bool IsZero(T value, T epsilon = EPSILON<T>) noexcept
        {
            return std::abs(value) <= epsilon;
        }

        /**
         * @brief Gradually changes a value towards a target
         * @param current The current value
         * @param target The target value
         * @param rate The interpolation rate (0-1)
         * @return The changed value
         */
        template <Arithmetic T, FloatingPoint K = float>
        static constexpr auto SmoothDamp(T current, T target, K rate) noexcept
        {
            return current + (target - current) * rate;
        }

        /**
         * @brief Linear interpolation between two values
         * @param a The first value
         * @param b The second value
         * @param t The interpolation factor (0-1)
         * @return The interpolated value
         */
        template <Arithmetic T, FloatingPoint K = float>
        static constexpr auto Lerp(T a, T b, K t) noexcept
        {
            return a + (b - a) * t;
        }

        /**
         * @brief Inverse linear interpolation (find t given a, b, and value)
         * @param a The first value
         * @param b The second value
         * @param value The value between a and b
         * @return The interpolation factor
         */
        template <Arithmetic T>
        static constexpr auto InverseLerp(T a, T b, T value) noexcept
        {
            using Result = std::conditional_t<std::is_floating_point_v<T>, T, double>;
            if (a == b)
                return Result(0);
            return (value - a) / static_cast<Result>(b - a);
        }

        /**
         * @brief Cosine interpolation between two values
         * @param a The first value
         * @param b The second value
         * @param t The blend factor (0-1)
         * @return The interpolated value
         */
        template <Arithmetic T, FloatingPoint K = float>
        static auto CosLerp(T a, T b, K t)
        {
            K mu = (K(1) - std::cos(t * PI<K>)) / K(2);
            return Lerp(a, b, mu);
        }

        /**
         * @brief Smooth Hermite interpolation (smoothstep)
         * @param edge0 The lower edge
         * @param edge1 The upper edge
         * @param x The value to interpolate
         * @return The smoothly interpolated value
         */
        template <FloatingPoint T = float>
        static constexpr T Smoothstep(T edge0, T edge1, T x) noexcept
        {
            T t = std::clamp((x - edge0) / (edge1 - edge0), T(0), T(1));
            return t * t * (T(3) - T(2) * t);
        }

        /**
         * @brief Smoother interpolation than smoothstep (smootherstep)
         * @param edge0 The lower edge
         * @param edge1 The upper edge
         * @param x The value to interpolate
         * @return The smoothly interpolated value
         */
        template <FloatingPoint T = float>
        static constexpr T Smootherstep(T edge0, T edge1, T x) noexcept
        {
            T t = std::clamp((x - edge0) / (edge1 - edge0), T(0), T(1));
            return t * t * t * (t * (t * T(6) - T(15)) + T(10));
        }

        /**
         * @brief Remap a value from one range to another
         * @param value The value to remap
         * @param fromMin The source range minimum
         * @param fromMax The source range maximum
         * @param toMin The target range minimum
         * @param toMax The target range maximum
         * @return The remapped value
         */
        template <Arithmetic T>
        static constexpr auto Remap(T value, T fromMin, T fromMax, T toMin, T toMax) noexcept
        {
            using Result = std::conditional_t<std::is_floating_point_v<T>, T, double>;
            Result t = InverseLerp(fromMin, fromMax, value);
            return Lerp(toMin, toMax, t);
        }

        /**
         * @brief Clamp a value between min and max
         * @param value The value to clamp
         * @param min The minimum value
         * @param max The maximum value
         * @return The clamped value
         */
        template <Arithmetic T>
        static constexpr T Clamp(T value, T min, T max) noexcept
        {
            return std::clamp(value, min, max);
        }

        /**
         * @brief Clamp a value between 0 and 1
         * @param value The value to clamp
         * @return The clamped value
         */
        template <Arithmetic T>
        static constexpr auto Saturate(T value) noexcept
        {
            using Result = std::conditional_t<std::is_floating_point_v<T>, T, double>;
            return std::clamp(static_cast<Result>(value), Result(0), Result(1));
        }

        /**
         * @brief Calculate cosine from sine and angle
         * @param sin The sine value
         * @param angle The angle in radians
         * @return The cosine value
         */
        template <FloatingPoint T = float>
        static T CosFromSin(T sin, T angle)
        {
            T cos = std::sqrt(T(1) - sin * sin);
            T normalizedAngle = WrapRadians(angle + PI<T> / T(2));
            return (normalizedAngle >= PI<T>) ? -cos : cos;
        }

        /**
         * @brief Fast power function for integer exponents
         * @param base The base value
         * @param exponent The integer exponent
         * @return base^exponent
         */
        template <Arithmetic T>
        static constexpr T Pow(T base, int exponent) noexcept
        {
            if (exponent == 0)
                return T(1);
            if (exponent < 0)
                return T(1) / Pow(base, -exponent);

            T result = T(1);
            T currentPower = base;

            while (exponent > 0)
            {
                if (exponent & 1)
                    result *= currentPower;
                currentPower *= currentPower;
                exponent >>= 1;
            }

            return result;
        }

        /**
         * @brief Sign function (-1, 0, or 1)
         * @param value The value
         * @return -1 if negative, 0 if zero, 1 if positive
         */
        template <Arithmetic T>
        static constexpr int Sign(T value) noexcept
        {
            return (T(0) < value) - (value < T(0));
        }

        /**
         * @brief Ping-pong value between 0 and length
         * @param t The value
         * @param length The length of the ping-pong
         * @return The ping-ponged value
         */
        template <FloatingPoint T = float>
        static T PingPong(T t, T length) noexcept
        {
            t = std::fmod(t, length * T(2));
            return length - std::abs(t - length);
        }

        /**
         * @brief Move towards a target value with max delta
         * @param current Current value
         * @param target Target value
         * @param maxDelta Maximum change per call
         * @return The new value
         */
        template <Arithmetic T>
        static constexpr T MoveTowards(T current, T target, T maxDelta) noexcept
        {
            if (std::abs(target - current) <= maxDelta)
                return target;
            return current + Sign(target - current) * maxDelta;
        }

        /**
         * @brief Combines a seed with a hash value
         * @param seed The seed to modify
         * @param v The value to hash
         */
        template <typename T>
        static void HashCombine(std::size_t &seed, const T &v) noexcept
        {
            std::hash<T> hasher;
            seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        /**
         * @brief Hash multiple values together
         * @param args Values to hash
         * @return Combined hash
         */
        template <typename... Args>
        static std::size_t Hash(const Args &...args) noexcept
        {
            std::size_t seed = 0;
            (HashCombine(seed, args), ...);
            return seed;
        }
    };
}