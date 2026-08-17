#pragma once

#include <TemplateLibrary/TypeTraits.hpp>

namespace SF::Engine::Bitmask
{
    // Wrapper type for enum Bitmasks
    template <typename Enum, typename = ::SFTL::enable_if_t<::SFTL::is_enum_v<Enum>>>
    class Bitmask
    {
    public:
        using underlying_type = ::SFTL::underlying_type_t<Enum>;

        constexpr Bitmask() noexcept : value(0) {}
        constexpr Bitmask(Enum e) noexcept : value(static_cast<underlying_type>(e)) {}
        constexpr explicit Bitmask(underlying_type v) noexcept : value(v) {}

        // Conversion to underlying type
        constexpr explicit operator underlying_type() const noexcept { return value; }

        // Bitwise operations
        constexpr Bitmask& operator|=(Enum e) noexcept
        {
            value |= static_cast<underlying_type>(e);
            return *this;
        }

        constexpr Bitmask& operator&=(Enum e) noexcept
        {
            value &= static_cast<underlying_type>(e);
            return *this;
        }

        constexpr Bitmask& operator^=(Enum e) noexcept
        {
            value ^= static_cast<underlying_type>(e);
            return *this;
        }

        constexpr Bitmask& operator|=(const Bitmask& other) noexcept
        {
            value |= other.value;
            return *this;
        }

        constexpr Bitmask& operator&=(const Bitmask& other) noexcept
        {
            value &= other.value;
            return *this;
        }

        constexpr Bitmask& operator^=(const Bitmask& other) noexcept
        {
            value ^= other.value;
            return *this;
        }

        // Bitwise NOT
        constexpr Bitmask operator~() const noexcept
        {
            return Bitmask(~value);
        }

        // Conversion to bool
        constexpr explicit operator bool() const noexcept { return value != 0; }

        // Get raw value
        constexpr underlying_type get() const noexcept { return value; }

    private:
        underlying_type value;
    };

    // Operator overloads for enum + Bitmask
    template <typename Enum>
    constexpr Bitmask<Enum> operator|(Enum lhs, Enum rhs) noexcept
    {
        return Bitmask<Enum>(lhs) |= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator&(Enum lhs, Enum rhs) noexcept
    {
        return Bitmask<Enum>(lhs) &= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator^(Enum lhs, Enum rhs) noexcept
    {
        return Bitmask<Enum>(lhs) ^= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator|(Bitmask<Enum> lhs, Enum rhs) noexcept
    {
        return lhs |= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator&(Bitmask<Enum> lhs, Enum rhs) noexcept
    {
        return lhs &= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator^(Bitmask<Enum> lhs, Enum rhs) noexcept
    {
        return lhs ^= rhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator|(Enum lhs, Bitmask<Enum> rhs) noexcept
    {
        return rhs | lhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator&(Enum lhs, Bitmask<Enum> rhs) noexcept
    {
        return rhs & lhs;
    }

    template <typename Enum>
    constexpr Bitmask<Enum> operator^(Enum lhs, Bitmask<Enum> rhs) noexcept
    {
        return rhs ^ lhs;
    }

    // Comparison operators
    template <typename Enum>
    constexpr bool operator==(const Bitmask<Enum>& lhs, const Bitmask<Enum>& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    template <typename Enum>
    constexpr bool operator!=(const Bitmask<Enum>& lhs, const Bitmask<Enum>& rhs) noexcept
    {
        return lhs.get() != rhs.get();
    }

    template <typename Enum>
    constexpr bool operator==(const Bitmask<Enum>& lhs, Enum rhs) noexcept
    {
        return lhs.get() == static_cast<typename Bitmask<Enum>::underlying_type>(rhs);
    }

    template <typename Enum>
    constexpr bool operator!=(const Bitmask<Enum>& lhs, Enum rhs) noexcept
    {
        return lhs.get() != static_cast<typename Bitmask<Enum>::underlying_type>(rhs);
    }

    template <typename Enum>
    constexpr bool operator==(Enum lhs, const Bitmask<Enum>& rhs) noexcept
    {
        return rhs == lhs;
    }

    template <typename Enum>
    constexpr bool operator!=(Enum lhs, const Bitmask<Enum>& rhs) noexcept
    {
        return rhs != lhs;
    }

    // Check if a specific flag is set
    template <typename Enum>
    constexpr bool is_set(const Bitmask<Enum>& mask, Enum flag) noexcept
    {
        return (mask.get() & static_cast<typename Bitmask<Enum>::underlying_type>(flag)) != 0;
    }

    // Set a flag
    template <typename Enum>
    constexpr Bitmask<Enum> set(Bitmask<Enum> mask, Enum flag) noexcept
    {
        return mask | flag;
    }

    // Clear a flag
    template <typename Enum>
    constexpr Bitmask<Enum> clear(Bitmask<Enum> mask, Enum flag) noexcept
    {
        return Bitmask<Enum>(mask.get() & ~static_cast<typename Bitmask<Enum>::underlying_type>(flag));
    }

    // Toggle a flag
    template <typename Enum>
    constexpr Bitmask<Enum> toggle(Bitmask<Enum> mask, Enum flag) noexcept
    {
        return mask ^ flag;
    }

    // Create mask from multiple flags
    template <typename Enum, typename... Enums>
    constexpr Bitmask<Enum> make_mask(Enum first, Enums... rest) noexcept
    {
        using underlying_type = typename Bitmask<Enum>::underlying_type;
        underlying_type result = static_cast<underlying_type>(first);
        ((result |= static_cast<underlying_type>(rest)), ...);
        return Bitmask<Enum>(result);
    }
}

// Helper macro to enable Bitmask operations on an enum class
#define ENABLE_BITMASK_OPERATORS(Enum) \
    namespace SF::Engine::Bitmask { \
        inline constexpr Bitmask<Enum> operator|(Enum lhs, Enum rhs) noexcept \
        { \
            return Bitmask<Enum>(lhs) |= rhs; \
        } \
        inline constexpr Bitmask<Enum> operator&(Enum lhs, Enum rhs) noexcept \
        { \
            return Bitmask<Enum>(lhs) &= rhs; \
        } \
        inline constexpr Bitmask<Enum> operator^(Enum lhs, Enum rhs) noexcept \
        { \
            return Bitmask<Enum>(lhs) ^= rhs; \
        } \
    }