#pragma once

#include <cassert>
#include <sstream>
#include <iomanip>
#include <array>
#include <cmath>
#include <string_view>

#include <Math/Math.hpp>

namespace SF::Engine
{
    /**
     * @brief Represents an RGBA color with floating-point components (0-1 range)
     */
    class Color
    {
    public:
        /**
         * @brief Component packing order for integer conversion
         */
        enum class PackingOrder : uint8_t
        {
            RGBA, // Red [31-24], Green [23-16], Blue [15-8], Alpha [7-0]
            ARGB, // Alpha [31-24], Red [23-16], Green [15-8], Blue [7-0]
            BGRA, // Blue [31-24], Green [23-16], Red [15-8], Alpha [7-0]
            ABGR, // Alpha [31-24], Blue [23-16], Green [15-8], Red [7-0]
            RGB   // Red [23-16], Green [15-8], Blue [7-0], Alpha = 1.0
        };

        /**
         * @brief Default constructor (transparent black)
         */
        constexpr Color() noexcept = default;

        /**
         * @brief Constructor from individual components
         * @param r Red component (0-1)
         * @param g Green component (0-1)
         * @param b Blue component (0-1)
         * @param a Alpha component (0-1)
         */
        constexpr Color(float r, float g, float b, float a = 1.0f) noexcept
            : r(r), g(g), b(b), a(a) {}

        /**
         * @brief Constructor from integer value
         * @param value The packed integer value
         * @param order The component packing order
         */
        constexpr Color(uint32_t value, PackingOrder order = PackingOrder::RGB) noexcept
        {
            FromInt(value, order);
        }

        /**
         * @brief Constructor from hex string (e.g., "#FF0000" or "FF0000")
         * @param hex The hex string
         * @param alpha Optional alpha value
         */
        explicit Color(std::string_view hex, float alpha = 1.0f)
            : a(alpha)
        {
            FromHex(hex);
        }

        /**
         * @brief Create color from HSV values
         * @param hue Hue (0-360)
         * @param saturation Saturation (0-1)
         * @param value Value/brightness (0-1)
         * @param alpha Alpha (0-1)
         * @return The color
         */
        static Color FromHSV(float hue, float saturation, float value, float alpha = 1.0f);

        /**
         * @brief Create color from HSL values
         * @param hue Hue (0-360)
         * @param saturation Saturation (0-1)
         * @param lightness Lightness (0-1)
         * @param alpha Alpha (0-1)
         * @return The color
         */
        static Color FromHSL(float hue, float saturation, float lightness, float alpha = 1.0f);

        /**
         * @brief Linear interpolation between two colors
         * @param other The other color
         * @param t The interpolation factor (0-1)
         * @return The interpolated color
         */
        constexpr Color Lerp(const Color &other, float t) const noexcept
        {
            return Color(
                Maths::Lerp(r, other.r, t),
                Maths::Lerp(g, other.g, t),
                Maths::Lerp(b, other.b, t),
                Maths::Lerp(a, other.a, t));
        }

        /**
         * @brief Smooth interpolation between two colors
         * @param other The other color
         * @param t The interpolation factor (0-1)
         * @return The interpolated color
         */
        Color SmoothLerp(const Color &other, float t) const
        {
            float smoothT = Maths::Smoothstep(0.0f, 1.0f, t);
            return Lerp(other, smoothT);
        }

        /**
         * @brief Normalize the color (make it unit length in 4D space)
         * @return The normalized color
         */
        [[nodiscard]] Color Normalize() const
        {
            float len = Length();
            if (Maths::IsZero(len))
                return Color(0, 0, 0, 0);
            return *this / len;
        }

        /**
         * @brief Get the squared length of the color vector
         * @return The squared length
         */
        [[nodiscard]] constexpr float LengthSquared() const noexcept
        {
            return r * r + g * g + b * b + a * a;
        }

        /**
         * @brief Get the length of the color vector
         * @return The length
         */
        [[nodiscard]] float Length() const noexcept
        {
            return std::sqrt(LengthSquared());
        }

        /**
         * @brief Gradually change towards a target color
         * @param target The target color
         * @param rate The interpolation rate
         * @return The changed color
         */
        constexpr Color SmoothDamp(const Color &target, float rate) const noexcept
        {
            return Lerp(target, rate);
        }

        /**
         * @brief Convert to grayscale using luminance
         * @return Grayscale color
         */
        [[nodiscard]] Color ToGrayscale() const noexcept
        {
            float luminance = r * 0.299f + g * 0.587f + b * 0.114f;
            return Color(luminance, luminance, luminance, a);
        }

        /**
         * @brief Invert the color (1 - component)
         * @param invertAlpha Whether to invert alpha as well
         * @return The inverted color
         */
        [[nodiscard]] constexpr Color Invert(bool invertAlpha = false) const noexcept
        {
            return Color(
                1.0f - r,
                1.0f - g,
                1.0f - b,
                invertAlpha ? 1.0f - a : a);
        }

        /**
         * @brief Clamp all components to 0-1 range
         * @return The clamped color
         */
        [[nodiscard]] Color Saturate() const noexcept
        {
            return Color(
                Maths::Saturate(r),
                Maths::Saturate(g),
                Maths::Saturate(b),
                Maths::Saturate(a));
        }

        /**
         * @brief Get HSV representation
         * @return Array [hue (0-360), saturation (0-1), value (0-1)]
         */
        [[nodiscard]] std::array<float, 3> ToHSV() const;

        /**
         * @brief Get HSL representation
         * @return Array [hue (0-360), saturation (0-1), lightness (0-1)]
         */
        [[nodiscard]] std::array<float, 3> ToHSL() const;

        /**
         * @brief Convert to packed integer
         * @param order The component packing order
         * @return The packed integer
         */
        [[nodiscard]] constexpr uint32_t ToInt(PackingOrder order = PackingOrder::RGBA) const noexcept
        {
            auto r8 = static_cast<uint8_t>(Maths::Saturate(r) * 255.0f);
            auto g8 = static_cast<uint8_t>(Maths::Saturate(g) * 255.0f);
            auto b8 = static_cast<uint8_t>(Maths::Saturate(b) * 255.0f);
            auto a8 = static_cast<uint8_t>(Maths::Saturate(a) * 255.0f);

            switch (order)
            {
            case PackingOrder::RGBA:
                return (r8 << 24) | (g8 << 16) | (b8 << 8) | a8;
            case PackingOrder::ARGB:
                return (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
            case PackingOrder::BGRA:
                return (b8 << 24) | (g8 << 16) | (r8 << 8) | a8;
            case PackingOrder::ABGR:
                return (a8 << 24) | (b8 << 16) | (g8 << 8) | r8;
            case PackingOrder::RGB:
                return (r8 << 16) | (g8 << 8) | b8;
            default:
                return 0;
            }
        }

        /**
         * @brief Convert to hex string
         * @param includeAlpha Whether to include alpha in the string
         * @return The hex string (e.g., "#FF0000" or "#FF0000FF")
         */
        [[nodiscard]] std::string ToHex(bool includeAlpha = false) const
        {
            std::stringstream ss;
            ss << "#" << std::hex << std::setfill('0') << std::uppercase;
            ss << std::setw(2) << static_cast<int>(r * 255.0f);
            ss << std::setw(2) << static_cast<int>(g * 255.0f);
            ss << std::setw(2) << static_cast<int>(b * 255.0f);
            if (includeAlpha)
                ss << std::setw(2) << static_cast<int>(a * 255.0f);
            return ss.str();
        }

        /**
         * @brief Component access by index
         */
        [[nodiscard]] constexpr float operator[](size_t i) const
        {
            assert(i < 4 && "Color subscript out of range");
            switch (i)
            {
            case 0:
                return r;
            case 1:
                return g;
            case 2:
                return b;
            case 3:
                return a;
            default:
                return 0.0f;
            }
        }

        // No constexpr here :(
        [[nodiscard]] float &operator[](size_t i) /* constexpr is causing dummy to throw "variable in constexpr function does not have automatic storage duration" */
        {
            assert(i < 4 && "Color subscript out of range");
            switch (i)
            {
            case 0:
                return r;
            case 1:
                return g;
            case 2:
                return b;
            case 3:
                return a;
            default:
                static float dummy = 0.0f;
                return dummy;
            }
        }

        // Comparison operators
        [[nodiscard]] constexpr bool operator==(const Color &rhs) const noexcept;
        [[nodiscard]] constexpr bool operator!=(const Color &rhs) const noexcept;

        // Arithmetic operators
        friend constexpr Color operator+(const Color &lhs, const Color &rhs) noexcept;
        friend constexpr Color operator-(const Color &lhs, const Color &rhs) noexcept;
        friend constexpr Color operator*(const Color &lhs, const Color &rhs) noexcept;
        friend constexpr Color operator/(const Color &lhs, const Color &rhs) noexcept;

        friend constexpr Color operator*(const Color &lhs, float rhs) noexcept;
        friend constexpr Color operator*(float lhs, const Color &rhs) noexcept;
        friend constexpr Color operator/(const Color &lhs, float rhs) noexcept;

        constexpr Color &operator+=(const Color &rhs) noexcept;
        constexpr Color &operator-=(const Color &rhs) noexcept;
        constexpr Color &operator*=(const Color &rhs) noexcept;
        constexpr Color &operator/=(const Color &rhs) noexcept;
        constexpr Color &operator*=(float rhs) noexcept;
        constexpr Color &operator/=(float rhs) noexcept;

        friend std::ostream &operator<<(std::ostream &stream, const Color &color);

        // Predefined colors
        static const Color Clear;
        static const Color Black;
        static const Color White;
        static const Color Red;
        static const Color Green;
        static const Color Blue;
        static const Color Yellow;
        static const Color Cyan;
        static const Color Magenta;
        static const Color Orange;
        static const Color Purple;
        static const Color Pink;
        static const Color Brown;
        static const Color Gray;
        static const Color LightGray;
        static const Color DarkGray;

        // Web colors
        static const Color Maroon;
        static const Color Olive;
        static const Color Lime;
        static const Color Aqua;
        static const Color Teal;
        static const Color Navy;
        static const Color Fuchsia;
        static const Color Silver;

        float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    private:
        constexpr void FromInt(uint32_t value, PackingOrder order) noexcept
        {
            switch (order)
            {
            case PackingOrder::RGBA:
                r = ((value >> 24) & 0xFF) / 255.0f;
                g = ((value >> 16) & 0xFF) / 255.0f;
                b = ((value >> 8) & 0xFF) / 255.0f;
                a = (value & 0xFF) / 255.0f;
                break;
            case PackingOrder::ARGB:
                a = ((value >> 24) & 0xFF) / 255.0f;
                r = ((value >> 16) & 0xFF) / 255.0f;
                g = ((value >> 8) & 0xFF) / 255.0f;
                b = (value & 0xFF) / 255.0f;
                break;
            case PackingOrder::BGRA:
                b = ((value >> 24) & 0xFF) / 255.0f;
                g = ((value >> 16) & 0xFF) / 255.0f;
                r = ((value >> 8) & 0xFF) / 255.0f;
                a = (value & 0xFF) / 255.0f;
                break;
            case PackingOrder::ABGR:
                a = ((value >> 24) & 0xFF) / 255.0f;
                b = ((value >> 16) & 0xFF) / 255.0f;
                g = ((value >> 8) & 0xFF) / 255.0f;
                r = (value & 0xFF) / 255.0f;
                break;
            case PackingOrder::RGB:
                r = ((value >> 16) & 0xFF) / 255.0f;
                g = ((value >> 8) & 0xFF) / 255.0f;
                b = (value & 0xFF) / 255.0f;
                a = 1.0f;
                break;
            }
        }

        void FromHex(std::string_view hex)
        {
            // Remove '#' if present
            if (!hex.empty() && hex[0] == '#')
                hex.remove_prefix(1);

            if (hex.size() != 6 && hex.size() != 8)
                throw std::invalid_argument("Hex string must be 6 or 8 characters (RGB or RGBA)");

            auto hexValue = std::stoul(std::string(hex), nullptr, 16);

            if (hex.size() == 6)
            {
                r = ((hexValue >> 16) & 0xFF) / 255.0f;
                g = ((hexValue >> 8) & 0xFF) / 255.0f;
                b = (hexValue & 0xFF) / 255.0f;
            }
            else // 8 characters (RGBA)
            {
                r = ((hexValue >> 24) & 0xFF) / 255.0f;
                g = ((hexValue >> 16) & 0xFF) / 255.0f;
                b = ((hexValue >> 8) & 0xFF) / 255.0f;
                a = (hexValue & 0xFF) / 255.0f;
            }
        }
    };
}

#include "Color.inl"