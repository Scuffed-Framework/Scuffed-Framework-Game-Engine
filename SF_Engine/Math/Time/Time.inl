#pragma once

#include "Time.hpp"

namespace SF::Engine
{
    // Unary operators
    constexpr ApplicationTime ApplicationTime::operator-() const noexcept
    {
        return ApplicationTime(-m_value);
    }

    constexpr ApplicationTime ApplicationTime::operator+() const noexcept
    {
        return *this;
    }

    // Arithmetic operators
    constexpr ApplicationTime operator+(const ApplicationTime &lhs, const ApplicationTime &rhs) noexcept
    {
        return ApplicationTime(lhs.m_value + rhs.m_value);
    }

    constexpr ApplicationTime operator-(const ApplicationTime &lhs, const ApplicationTime &rhs) noexcept
    {
        return ApplicationTime(lhs.m_value - rhs.m_value);
    }

    template <std::floating_point T>
    constexpr ApplicationTime operator*(const ApplicationTime &lhs, T rhs) noexcept
    {
        return ApplicationTime(std::chrono::duration_cast<ApplicationTime::Duration>(
            std::chrono::duration<double, std::micro>(lhs.m_value.count() * rhs)));
    }

    template <std::integral T>
    constexpr ApplicationTime operator*(const ApplicationTime &lhs, T rhs) noexcept
    {
        return ApplicationTime(lhs.m_value * rhs);
    }

    template <std::floating_point T>
    constexpr ApplicationTime operator*(T lhs, const ApplicationTime &rhs) noexcept
    {
        return rhs * lhs;
    }

    template <std::integral T>
    constexpr ApplicationTime operator*(T lhs, const ApplicationTime &rhs) noexcept
    {
        return rhs * lhs;
    }

    template <std::floating_point T>
    constexpr ApplicationTime operator/(const ApplicationTime &lhs, T rhs) noexcept
    {
        return ApplicationTime(std::chrono::duration_cast<ApplicationTime::Duration>(
            std::chrono::duration<double, std::micro>(lhs.m_value.count() / rhs)));
    }

    template <std::integral T>
    constexpr ApplicationTime operator/(const ApplicationTime &lhs, T rhs) noexcept
    {
        return ApplicationTime(lhs.m_value / rhs);
    }

    constexpr double operator/(const ApplicationTime &lhs, const ApplicationTime &rhs) noexcept
    {
        return static_cast<double>(lhs.m_value.count()) /
               static_cast<double>(rhs.m_value.count());
    }

    // Compound assignment operators
    constexpr ApplicationTime &ApplicationTime::operator+=(const ApplicationTime &rhs) noexcept
    {
        m_value += rhs.m_value;
        return *this;
    }

    constexpr ApplicationTime &ApplicationTime::operator-=(const ApplicationTime &rhs) noexcept
    {
        m_value -= rhs.m_value;
        return *this;
    }

    template <typename T>
    constexpr ApplicationTime &ApplicationTime::operator*=(T rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

    template <typename T>
    constexpr ApplicationTime &ApplicationTime::operator/=(T rhs) noexcept
    {
        *this = *this / rhs;
        return *this;
    }

    // Stream operator
    inline std::ostream &operator<<(std::ostream &os, const ApplicationTime &time)
    {
        return os << time.ToString();
    }
}