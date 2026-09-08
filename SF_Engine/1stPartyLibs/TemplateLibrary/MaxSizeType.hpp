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

                template<typename _Tp>
                    requires integral<_Tp>
                explicit constexpr max_size_type(_Tp i) noexcept : Value(static_cast<s_uint128>(i))
                {
                }

                constexpr explicit max_size_type(const max_diff_type &d) noexcept;

                template<typename _Tp>
                    requires integral<_Tp>
                constexpr explicit operator _Tp() const noexcept
                {
                    return static_cast<_Tp>(Value);
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
                constexpr max_diff_type() noexcept : _M_rep(0) {}
                constexpr explicit max_diff_type(s_uint128 value) noexcept : _M_rep(value) {}

                template<typename _Tp>
                    requires integral<_Tp>
                constexpr max_diff_type(_Tp i) noexcept : _M_rep(static_cast<s_uint128>(i))
                {
                }

                constexpr explicit max_diff_type(const max_size_type &d) noexcept :
                    _M_rep(static_cast<s_uint128>(d.Value))
                {
                }

                template<typename _Tp>
                    requires integral<_Tp>
                constexpr explicit operator _Tp() const noexcept
                {
                    return static_cast<_Tp>(_M_rep);
                }

                constexpr explicit operator bool() const noexcept
                {
                    s_uint128 bruh = _M_rep;
                    return bruh != s_uint128(0, 0);
                }

                constexpr max_diff_type operator+() const noexcept { return *this; }

                constexpr max_diff_type operator~() const noexcept
                {
                    s_uint128 bruh = ~_M_rep;
                    return max_diff_type(bruh);
                }

                constexpr max_diff_type operator-() const noexcept
                {
                    s_uint128 bruh = -_M_rep;
                    return max_diff_type(bruh);
                }

                constexpr max_diff_type &operator++() noexcept
                {
                    ++_M_rep;
                    return *this;
                }
                constexpr max_diff_type operator++(int) noexcept
                {
                    auto tmp = *this;
                    ++_M_rep;
                    return tmp;
                }

                constexpr max_diff_type &operator--() noexcept
                {
                    --_M_rep;
                    return *this;
                }
                constexpr max_diff_type operator--(int) noexcept
                {
                    auto tmp = *this;
                    --_M_rep;
                    return tmp;
                }

                constexpr max_diff_type &operator+=(const max_diff_type &right) noexcept
                {
                    _M_rep += right._M_rep;
                    return *this;
                }
                constexpr max_diff_type &operator-=(const max_diff_type &right) noexcept
                {
                    _M_rep -= right._M_rep;
                    return *this;
                }
                constexpr max_diff_type &operator*=(const max_diff_type &right) noexcept
                {
                    _M_rep *= right._M_rep;
                    return *this;
                }
                constexpr max_diff_type &operator/=(const max_diff_type &right) noexcept
                {
                    _M_rep /= right._M_rep;
                    return *this;
                }
                constexpr max_diff_type &operator%=(const max_diff_type &right) noexcept
                {
                    _M_rep %= right._M_rep;
                    return *this;
                }

                friend constexpr bool operator==(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep == right._M_rep;
                }
                friend constexpr bool operator!=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep != right._M_rep;
                }
                friend constexpr bool operator<(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep < right._M_rep;
                }
                friend constexpr bool operator<=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep <= right._M_rep;
                }
                friend constexpr bool operator>(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep > right._M_rep;
                }
                friend constexpr bool operator>=(const max_diff_type &left, const max_diff_type &right) noexcept
                {
                    return left._M_rep >= right._M_rep;
                }

                s_uint128 _M_rep;
            };

            inline constexpr max_size_type::max_size_type(const max_diff_type &d) noexcept :
                Value(static_cast<s_uint128>(d._M_rep))
            {
            }

        } // namespace Detail
    } // namespace Ranges
} // namespace SFTL
