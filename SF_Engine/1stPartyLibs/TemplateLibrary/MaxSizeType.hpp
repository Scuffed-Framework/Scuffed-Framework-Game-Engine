#pragma once
#include "NumericProperties.hpp"
#include "Types.hpp"

namespace SFTL
{
    namespace Ranges
    {
        namespace Detail
        {
            class max_diff_type;

            class max_size_type
            {
            public:
                constexpr max_size_type() noexcept : Value(0) {}
                explicit constexpr max_size_type(s_uint128 value) noexcept : Value(value) {}

                template<typename Type>
                    requires integral<Type>
                explicit constexpr max_size_type(Type i) noexcept : Value(static_cast<s_uint128>(i))
                {
                }

                constexpr explicit max_size_type(const max_diff_type &d) noexcept;

                template<typename Type>
                    requires integral<Type>
                constexpr explicit operator Type() const noexcept
                {
                    return static_cast<Type>(Value);
                }

                constexpr explicit operator bool() const noexcept
                {
                    s_uint128 res = ~Value;
                    return res != s_uint128(static_cast<uint64>(0), static_cast<uint64>(0));
                }

                constexpr max_size_type operator+() const noexcept { return *this; }

                constexpr max_size_type operator~() const noexcept
                {
                    s_uint128 bruh = ~Value;
                    return max_size_type(bruh);
                }

                constexpr max_size_type operator-() const noexcept
                {
                    s_uint128 bruh = -Value;
                    return max_size_type(bruh);
                }

                constexpr max_size_type &operator++() noexcept
                {
                    ++Value;
                    return *this;
                }
                constexpr max_size_type operator++(int) noexcept
                {
                    auto tmp = *this;
                    ++Value;
                    return tmp;
                }

                constexpr max_size_type &operator--() noexcept
                {
                    --Value;
                    return *this;
                }
                constexpr max_size_type operator--(int) noexcept
                {
                    auto tmp = *this;
                    --Value;
                    return tmp;
                }

                constexpr max_size_type &operator+=(const max_size_type &right) noexcept
                {
                    Value += right.Value;
                    return *this;
                }
                constexpr max_size_type &operator-=(const max_size_type &right) noexcept
                {
                    Value -= right.Value;
                    return *this;
                }
                constexpr max_size_type &operator*=(const max_size_type &right) noexcept
                {
                    Value *= right.Value;
                    return *this;
                }
                constexpr max_size_type &operator/=(const max_size_type &right) noexcept
                {
                    Value /= right.Value;
                    return *this;
                }
                constexpr max_size_type &operator%=(const max_size_type &right) noexcept
                {
                    Value %= right.Value;
                    return *this;
                }

                constexpr max_size_type &operator<<=(const max_size_type &right) noexcept
                {
                    Value <<= static_cast<unsigned>(right.Value.low);
                    return *this;
                }
                constexpr max_size_type &operator>>=(const max_size_type &right) noexcept
                {
                    Value >>= static_cast<unsigned>(right.Value.low);
                    return *this;
                }

                constexpr max_size_type &operator&=(const max_size_type &right) noexcept
                {
                    Value &= right.Value;
                    return *this;
                }
                constexpr max_size_type &operator|=(const max_size_type &right) noexcept
                {
                    Value |= right.Value;
                    return *this;
                }
                constexpr max_size_type &operator^=(const max_size_type &right) noexcept
                {
                    Value ^= right.Value;
                    return *this;
                }

                friend constexpr max_size_type operator+(max_size_type left, const max_size_type &right) noexcept
                {
                    return max_size_type(left.Value + right.Value);
                }
                friend constexpr max_size_type operator-(max_size_type left, const max_size_type &right) noexcept
                {
                    return max_size_type(left.Value - right.Value);
                }
                friend constexpr max_size_type operator*(max_size_type left, const max_size_type &right) noexcept
                {
                    return max_size_type(left.Value * right.Value);
                }
                friend constexpr max_size_type operator/(max_size_type left, const max_size_type &right) noexcept
                {
                    return max_size_type(left.Value / right.Value);
                }
                friend constexpr max_size_type operator%(max_size_type left, const max_size_type &right) noexcept
                {
                    return max_size_type(left.Value % right.Value);
                }

                friend constexpr bool operator==(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value == right.Value;
                }
                friend constexpr bool operator!=(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value != right.Value;
                }
                friend constexpr bool operator<(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value < right.Value;
                }
                friend constexpr bool operator<=(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value <= right.Value;
                }
                friend constexpr bool operator>(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value > right.Value;
                }
                friend constexpr bool operator>=(const max_size_type &left, const max_size_type &right) noexcept
                {
                    return left.Value >= right.Value;
                }

                s_uint128 Value;

                friend class max_diff_type;
            };

            class max_diff_type
            {
            public:
                constexpr max_diff_type() noexcept : Lrep(0) {}
                constexpr explicit max_diff_type(s_uint128 value) noexcept : Lrep(value) {}

                template<typename Type>
                    requires integral<Type>
                constexpr max_diff_type(Type i) noexcept : Lrep(static_cast<s_uint128>(i))
                {
                }

                constexpr explicit max_diff_type(const max_size_type &d) noexcept :
                    Lrep(static_cast<s_uint128>(d.Value))
                {
                }

                template<typename Type>
                    requires integral<Type>
                constexpr explicit operator Type() const noexcept
                {
                    return static_cast<Type>(Lrep);
                }

                constexpr explicit operator bool() const noexcept
                {
                    s_uint128 bruh = Lrep;
                    return bruh != s_uint128(0, 0);
                }

                constexpr max_diff_type operator+() const noexcept { return *this; }

                constexpr max_diff_type operator~() const noexcept
                {
                    s_uint128 bruh = ~Lrep;
                    return max_diff_type(bruh);
                }

                constexpr max_diff_type operator-() const noexcept
                {
                    s_uint128 bruh = -Lrep;
                    return max_diff_type(bruh);
                }

                constexpr max_diff_type &operator++() noexcept
                {
                    ++Lrep;
                    return *this;
                }
                constexpr max_diff_type operator++(int) noexcept
                {
                    auto tmp = *this;
                    ++Lrep;
                    return tmp;
                }

                constexpr max_diff_type &operator--() noexcept
                {
                    --Lrep;
                    return *this;
                }
                constexpr max_diff_type operator--(int) noexcept
                {
                    auto tmp = *this;
                    --Lrep;
                    return tmp;
                }

                constexpr max_diff_type &operator+=(const max_diff_type &right) noexcept
                {
                    Lrep += right.Lrep;
                    return *this;
                }
                constexpr max_diff_type &operator-=(const max_diff_type &right) noexcept
                {
                    Lrep -= right.Lrep;
                    return *this;
                }
                constexpr max_diff_type &operator*=(const max_diff_type &right) noexcept
                {
                    Lrep *= right.Lrep;
                    return *this;
                }
                constexpr max_diff_type &operator/=(const max_diff_type &right) noexcept
                {
                    Lrep /= right.Lrep;
                    return *this;
                }
                constexpr max_diff_type &operator%=(const max_diff_type &right) noexcept
                {
                    Lrep %= right.Lrep;
                    return *this;
                }

                friend constexpr bool operator==(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep == right.Lrep;
                }
                friend constexpr bool operator!=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep != right.Lrep;
                }
                friend constexpr bool operator<(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep < right.Lrep;
                }
                friend constexpr bool operator<=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep <= right.Lrep;
                }
                friend constexpr bool operator>(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep > right.Lrep;
                }
                friend constexpr bool operator>=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left.Lrep >= right.Lrep;
                }

                s_uint128 Lrep;
            };

            inline constexpr max_size_type::max_size_type(const max_diff_type &d) noexcept :
                Value(static_cast<s_uint128>(d.Lrep))
            {
            }

        } // namespace Detail
    } // namespace Ranges
} // namespace SFTL
