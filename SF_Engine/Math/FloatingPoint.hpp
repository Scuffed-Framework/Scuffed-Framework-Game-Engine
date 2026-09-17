
#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

namespace SF::Engine
{
    using namespace std;

    inline uint8_t bit_set_to(uint8_t number, uint8_t n, bool x)
    {
        return static_cast<uint8_t>((number & ~(static_cast<uint8_t>(1) << n)) | (static_cast<uint8_t>(x) << n));
    }

    namespace FloatDetail
    {
        using Word     = uint32_t;
        using WideWord = uint64_t;

        constexpr size_t WordBits = 32;

        template<size_t Bits>
        struct WordCount
        {
            static constexpr size_t Value = (Bits + 31) / 32;
        };

        template<size_t Bits>
        class BigUInt
        {
        public:
            static constexpr size_t Count = WordCount<Bits>::Value;

            array<Word, Count> words{};

            constexpr void Clear() noexcept { words.fill(0); }

            [[nodiscard]]
            constexpr bool IsZero() const noexcept
            {
                for (Word word: words)
                {
                    if (word != 0)
                        return false;
                }

                return true;
            }

            [[nodiscard]]
            constexpr bool GetBit(size_t index) const noexcept
            {
                if (index >= Bits)
                    return false;

                return (words[index / WordBits] >> (index % WordBits)) & 1u;
            }

            constexpr void SetBit(size_t index, bool value) noexcept
            {
                if (index >= Bits)
                    return;

                Word mask = Word(1) << (index % WordBits);

                if (value)
                    words[index / WordBits] |= mask;
                else
                    words[index / WordBits] &= ~mask;
            }

            constexpr void ShiftLeftOne() noexcept
            {
                Word carry = 0;

                for (size_t i = 0; i < Count; ++i)
                {
                    Word nextCarry = words[i] >> 31;
                    words[i]       = (words[i] << 1) | carry;
                    carry          = nextCarry;
                }

                Trim();
            }

            constexpr void ShiftRightOne() noexcept
            {
                Word carry = 0;

                for (size_t i = Count; i-- > 0;)
                {
                    Word nextCarry = words[i] & 1u;
                    words[i]       = (words[i] >> 1) | (carry << 31);
                    carry          = nextCarry;
                }
            }

            constexpr void Add(const BigUInt &other) noexcept
            {
                WideWord carry = 0;

                for (size_t i = 0; i < Count; ++i)
                {
                    WideWord sum = WideWord(words[i]) + WideWord(other.words[i]) + carry;

                    words[i] = Word(sum);
                    carry    = sum >> 32;
                }

                Trim();
            }

            constexpr bool Subtract(const BigUInt &other) noexcept
            {
                if (*this < other)
                    return false;

                WideWord borrow = 0;

                for (size_t i = 0; i < Count; ++i)
                {
                    WideWord lhs = words[i];
                    WideWord rhs = WideWord(other.words[i]) + borrow;

                    words[i] = Word(lhs - rhs);
                    borrow   = lhs < rhs;
                }

                return true;
            }

            [[nodiscard]]
            constexpr int Compare(const BigUInt &other) const noexcept
            {
                for (size_t i = Count; i-- > 0;)
                {
                    if (words[i] < other.words[i])
                        return -1;

                    if (words[i] > other.words[i])
                        return 1;
                }

                return 0;
            }

            constexpr void Trim() noexcept
            {
                if constexpr (Bits % WordBits != 0)
                {
                    constexpr Word mask = (Word(1) << (Bits % WordBits)) - 1;

                    words[Count - 1] &= mask;
                }
            }

            friend constexpr bool operator==(const BigUInt &a, const BigUInt &b) noexcept { return a.Compare(b) == 0; }

            friend constexpr bool operator<(const BigUInt &a, const BigUInt &b) noexcept { return a.Compare(b) < 0; }
        };

        enum class Special : uint8_t
        {
            Normal,
            Zero,
            Infinity,
            NaN
        };
    } // namespace FloatDetail

    template<int Bits>
        requires(Bits >= 2)
    class Float
    {
    private:
        using UInt     = FloatDetail::BigUInt<Bits>;
        using Word     = FloatDetail::Word;
        using WideWord = FloatDetail::WideWord;
        using Special  = FloatDetail::Special;

        static constexpr size_t LimbCount = UInt::Count;

        // The significand is stored as an integer with an implicit
        // binary point immediately after the most significant bit.
        //
        // A normalized value represents:
        //
        //     (-1)^negative * significand * 2^exponent
        //
        // where the significand is scaled by Bits bits.
        bool negative    = false;
        int64_t exponent = 0;
        UInt significand{};
        Special special = Special::Zero;

        static constexpr int64_t MaxExponent = numeric_limits<int64_t>::max();

        [[nodiscard]]
        static Float FromInteger(uint64_t value)
        {
            Float result;

            if (value == 0)
                return result;

            result.special = Special::Normal;

            while (value != 0)
            {
                result.significand.ShiftLeftOne();
                result.significand.SetBit(0, value & 1u);
                value >>= 1;
            }

            result.Normalize();
            return result;
        }

        void Normalize() noexcept
        {
            if (significand.IsZero())
            {
                special  = Special::Zero;
                negative = false;
                exponent = 0;
                return;
            }

            special = Special::Normal;

            // Find the most significant set bit.
            size_t highest = 0;

            for (size_t i = Bits; i-- > 0;)
            {
                if (significand.GetBit(i))
                {
                    highest = i;
                    break;
                }
            }

            // Place the highest bit at Bits - 1.
            while (highest < Bits - 1)
            {
                significand.ShiftLeftOne();
                --highest;
                --exponent;
            }

            while (highest > Bits - 1)
            {
                significand.ShiftRightOne();
                ++highest;
                ++exponent;
            }
        }

        [[nodiscard]]
        static int CompareMagnitude(const Float &a, const Float &b) noexcept
        {
            if (a.special == Special::Zero && b.special == Special::Zero)
                return 0;

            if (a.exponent < b.exponent)
                return -1;

            if (a.exponent > b.exponent)
                return 1;

            return a.significand.Compare(b.significand);
        }

    public:
        static constexpr int PrecisionBits = Bits;

        constexpr Float() noexcept = default;

        constexpr Float(const Float &) noexcept = default;
        constexpr Float(Float &&) noexcept      = default;

        constexpr Float &operator=(const Float &) noexcept = default;
        constexpr Float &operator=(Float &&) noexcept      = default;

        template<typename Type>
            requires is_integral_v<Type> && is_signed_v<Type>
        explicit Float(Type value)
        {
            if (value < 0)
            {
                negative = true;
                value    = -value;
            }

            *this = FromInteger(static_cast<uint64_t>(value));
        }

        template<typename Type>
            requires is_integral_v<Type> && is_unsigned_v<Type>
        explicit Float(Type value)
        {
            *this = FromInteger(static_cast<uint64_t>(value));
        }

        explicit Float(float value) { *this = FromDouble(static_cast<double>(value)); }

        explicit Float(double value) { *this = FromDouble(value); }

        [[nodiscard]]
        static Float FromDouble(double value)
        {
            Float result;

            if (isnan(value))
            {
                result.special = Special::NaN;
                return result;
            }

            if (isinf(value))
            {
                result.negative = signbit(value);
                result.special  = Special::Infinity;
                return result;
            }

            if (value == 0.0)
                return result;

            result.negative = signbit(value);
            value           = fabs(value);
            result.special  = Special::Normal;

            int exp         = 0;
            double fraction = frexp(value, &exp);

            // Convert the available double precision into our
            // arbitrary-precision significand.
            for (int i = 0; i < Bits; ++i)
            {
                fraction *= 2.0;

                result.significand.ShiftLeftOne();

                if (fraction >= 1.0)
                {
                    result.significand.SetBit(0, true);
                    fraction -= 1.0;
                }
            }

            result.exponent = exp - Bits;
            result.Normalize();

            return result;
        }

        [[nodiscard]]
        bool IsZero() const noexcept
        {
            return special == Special::Zero;
        }

        [[nodiscard]]
        bool IsNaN() const noexcept
        {
            return special == Special::NaN;
        }

        [[nodiscard]]
        bool IsInfinity() const noexcept
        {
            return special == Special::Infinity;
        }

        [[nodiscard]]
        bool IsFinite() const noexcept
        {
            return special == Special::Normal || special == Special::Zero;
        }

        [[nodiscard]]
        bool IsNegative() const noexcept
        {
            return negative;
        }

        [[nodiscard]]
        int64_t GetExponent() const noexcept
        {
            return exponent;
        }

        [[nodiscard]]
        const UInt &GetSignificand() const noexcept
        {
            return significand;
        }

        explicit operator double() const
        {
            if (special == Special::NaN)
                return numeric_limits<double>::quiet_NaN();

            if (special == Special::Infinity)
                return negative ? -numeric_limits<double>::infinity() : numeric_limits<double>::infinity();

            if (special == Special::Zero)
                return negative ? -0.0 : 0.0;

            double result = 0.0;

            for (size_t i = Bits; i-- > 0;)
            {
                result *= 2.0;

                if (significand.GetBit(i))
                    result += 1.0;
            }

            result = ldexp(result, static_cast<int>(exponent));

            return negative ? -result : result;
        }

        explicit operator float() const { return static_cast<float>(static_cast<double>(*this)); }

        [[nodiscard]]
        Float operator-() const
        {
            Float result    = *this;
            result.negative = !result.negative;
            return result;
        }

        [[nodiscard]]
        Float operator+() const
        {
            return *this;
        }

        friend Float operator+(const Float &lhs, const Float &rhs)
        {
            if (lhs.IsNaN() || rhs.IsNaN())
            {
                Float result;
                result.special = Special::NaN;
                return result;
            }

            if (lhs.IsZero())
                return rhs;

            if (rhs.IsZero())
                return lhs;

            if (lhs.negative != rhs.negative)
            {
                if (CompareMagnitude(lhs, rhs) >= 0)
                {
                    Float result = lhs;
                    result += -rhs;
                    return result;
                }

                Float result = rhs;
                result += -lhs;
                return result;
            }

            Float result = lhs;

            // Align exponents before adding significands.
            Float other = rhs;

            if (result.exponent < other.exponent)
                swap(result, other);

            int64_t difference = result.exponent - other.exponent;

            if (difference >= Bits)
                return result;

            for (int64_t i = 0; i < difference; ++i)
                other.significand.ShiftRightOne();

            result.significand.Add(other.significand);
            result.Normalize();

            return result;
        }

        friend Float operator-(const Float &lhs, const Float &rhs) { return lhs + (-rhs); }

        Float &operator+=(const Float &other)
        {
            *this = *this + other;
            return *this;
        }

        Float &operator-=(const Float &other)
        {
            *this = *this - other;
            return *this;
        }

        friend bool operator==(const Float &lhs, const Float &rhs)
        {
            if (lhs.IsNaN() || rhs.IsNaN())
                return false;

            if (lhs.IsZero() && rhs.IsZero())
                return true;

            return lhs.negative == rhs.negative && lhs.exponent == rhs.exponent && lhs.significand == rhs.significand;
        }

        friend bool operator!=(const Float &lhs, const Float &rhs) { return !(lhs == rhs); }

        friend bool operator<(const Float &lhs, const Float &rhs)
        {
            if (lhs.IsNaN() || rhs.IsNaN())
                return false;

            if (lhs.negative != rhs.negative)
                return lhs.negative;

            int magnitude = CompareMagnitude(lhs, rhs);

            return lhs.negative ? magnitude > 0 : magnitude < 0;
        }

        friend bool operator>(const Float &lhs, const Float &rhs) { return rhs < lhs; }

        friend bool operator<=(const Float &lhs, const Float &rhs) { return !(rhs < lhs); }

        friend bool operator>=(const Float &lhs, const Float &rhs) { return !(lhs < rhs); }

        friend ostream &operator<<(ostream &stream, const Float &value) { return stream << static_cast<double>(value); }
    };

    using Float16    = Float<16>;
    using Float32    = Float<32>;
    using Float64    = Float<64>;
    using Float128   = Float<128>;
    using Float256   = Float<256>;
    using Float512   = Float<512>;
    using Float1024  = Float<1024>;
    using Float2048  = Float<2048>;
    using Float4096  = Float<4096>;
    using Float8192  = Float<8192>;
    using Float16384 = Float<16384>;
} // namespace SF::Engine
