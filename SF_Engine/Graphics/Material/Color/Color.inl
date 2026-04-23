#pragma once

#include "Color.hpp"

namespace SF::Engine
{
    constexpr bool Color::operator==(const Color &rhs) const noexcept
    {
        return Maths::AlmostEqual(r, rhs.r) &&
               Maths::AlmostEqual(g, rhs.g) &&
               Maths::AlmostEqual(b, rhs.b) &&
               Maths::AlmostEqual(a, rhs.a);
    }

    constexpr bool Color::operator!=(const Color &rhs) const noexcept
    {
        return !(*this == rhs);
    }

    constexpr Color operator+(const Color &lhs, const Color &rhs) noexcept
    {
        return Color(lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b, lhs.a + rhs.a);
    }

    constexpr Color operator-(const Color &lhs, const Color &rhs) noexcept
    {
        return Color(lhs.r - rhs.r, lhs.g - rhs.g, lhs.b - rhs.b, lhs.a - rhs.a);
    }

    constexpr Color operator*(const Color &lhs, const Color &rhs) noexcept
    {
        return Color(lhs.r * rhs.r, lhs.g * rhs.g, lhs.b * rhs.b, lhs.a * rhs.a);
    }

    constexpr Color operator/(const Color &lhs, const Color &rhs) noexcept
    {
        return Color(lhs.r / rhs.r, lhs.g / rhs.g, lhs.b / rhs.b, lhs.a / rhs.a);
    }

    constexpr Color operator*(const Color &lhs, float rhs) noexcept
    {
        return Color(lhs.r * rhs, lhs.g * rhs, lhs.b * rhs, lhs.a * rhs);
    }

    constexpr Color operator*(float lhs, const Color &rhs) noexcept
    {
        return rhs * lhs;
    }

    constexpr Color operator/(const Color &lhs, float rhs) noexcept
    {
        return Color(lhs.r / rhs, lhs.g / rhs, lhs.b / rhs, lhs.a / rhs);
    }

    constexpr Color &Color::operator+=(const Color &rhs) noexcept
    {
        r += rhs.r;
        g += rhs.g;
        b += rhs.b;
        a += rhs.a;
        return *this;
    }

    constexpr Color &Color::operator-=(const Color &rhs) noexcept
    {
        r -= rhs.r;
        g -= rhs.g;
        b -= rhs.b;
        a -= rhs.a;
        return *this;
    }

    constexpr Color &Color::operator*=(const Color &rhs) noexcept
    {
        r *= rhs.r;
        g *= rhs.g;
        b *= rhs.b;
        a *= rhs.a;
        return *this;
    }

    constexpr Color &Color::operator/=(const Color &rhs) noexcept
    {
        r /= rhs.r;
        g /= rhs.g;
        b /= rhs.b;
        a /= rhs.a;
        return *this;
    }

    constexpr Color &Color::operator*=(float rhs) noexcept
    {
        r *= rhs;
        g *= rhs;
        b *= rhs;
        a *= rhs;
        return *this;
    }

    constexpr Color &Color::operator/=(float rhs) noexcept
    {
        r /= rhs;
        g /= rhs;
        b /= rhs;
        a /= rhs;
        return *this;
    }

    inline std::ostream &operator<<(std::ostream &stream, const Color &color)
    {
        return stream << "Color(" << color.r << ", " << color.g << ", "
                      << color.b << ", " << color.a << ")";
    }
}

namespace std
{
    template <>
    struct hash<SF::Engine::Color>
    {
        size_t operator()(const SF::Engine::Color &color) const noexcept
        {
            return SF::Engine::Maths::Hash(color.r, color.g, color.b, color.a);
        }
    };
}
