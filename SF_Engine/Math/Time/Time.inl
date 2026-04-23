#pragma once

#include "Time.hpp"

namespace SF::Engine
{
    // Unary operators
    constexpr Time Time::operator-() const noexcept
    {
        return Time(-m_value);
    }

    constexpr Time Time::operator+() const noexcept
    {
        return *this;
    }

    // Arithmetic operators
    constexpr Time operator+(const Time &lhs, const Time &rhs) noexcept
    {
        return Time(lhs.m_value + rhs.m_value);
    }

    constexpr Time operator-(const Time &lhs, const Time &rhs) noexcept
    {
        return Time(lhs.m_value - rhs.m_value);
    }

    template <std::floating_point T>
    constexpr Time operator*(const Time &lhs, T rhs) noexcept
    {
        return Time(std::chrono::duration_cast<Time::Duration>(
            std::chrono::duration<double, std::micro>(lhs.m_value.count() * rhs)));
    }

    template <std::integral T>
    constexpr Time operator*(const Time &lhs, T rhs) noexcept
    {
        return Time(lhs.m_value * rhs);
    }

    template <std::floating_point T>
    constexpr Time operator*(T lhs, const Time &rhs) noexcept
    {
        return rhs * lhs;
    }

    template <std::integral T>
    constexpr Time operator*(T lhs, const Time &rhs) noexcept
    {
        return rhs * lhs;
    }

    template <std::floating_point T>
    constexpr Time operator/(const Time &lhs, T rhs) noexcept
    {
        return Time(std::chrono::duration_cast<Time::Duration>(
            std::chrono::duration<double, std::micro>(lhs.m_value.count() / rhs)));
    }

    template <std::integral T>
    constexpr Time operator/(const Time &lhs, T rhs) noexcept
    {
        return Time(lhs.m_value / rhs);
    }

    constexpr double operator/(const Time &lhs, const Time &rhs) noexcept
    {
        return static_cast<double>(lhs.m_value.count()) /
               static_cast<double>(rhs.m_value.count());
    }

    // Compound assignment operators
    constexpr Time &Time::operator+=(const Time &rhs) noexcept
    {
        m_value += rhs.m_value;
        return *this;
    }

    constexpr Time &Time::operator-=(const Time &rhs) noexcept
    {
        m_value -= rhs.m_value;
        return *this;
    }

    template <typename T>
    constexpr Time &Time::operator*=(T rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

    template <typename T>
    constexpr Time &Time::operator/=(T rhs) noexcept
    {
        *this = *this / rhs;
        return *this;
    }

    // Stream operator
    inline std::ostream &operator<<(std::ostream &os, const Time &time)
    {
        return os << time.ToString();
    }
}