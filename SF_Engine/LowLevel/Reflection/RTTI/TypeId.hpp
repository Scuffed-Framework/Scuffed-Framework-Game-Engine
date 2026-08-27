#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <TemplateLibrary/TypeTraits.hpp>
#include <TemplateLibrary/Types.hpp>

#if defined(_MSC_VER)
#define SF_RTTI_FUNC_SIG __FUNCSIG__
#elif defined(__clang__) || defined(__GNUC__)
#define SF_RTTI_FUNC_SIG __PRETTY_FUNCTION__
#else
#error "SF Engine RTTI requires MSVC, Clang or GCC (needs a compiler-signature macro)."
#endif

namespace SF::RTTI
{
    using namespace ::SFTL;

    namespace Detail
    {
        inline constexpr uint64 FnvOffset = 14695981039346656037ull;
        inline constexpr uint64 FnvPrime = 1099511628211ull;

        // Iterative (not recursive) so it doesn't hit MSVC's constexpr
        // recursion-depth limit on long, heavily-templated signatures.
        constexpr uint64 Fnv1aHash(const char *str)
        {
            uint64 hash = FnvOffset;
            while (*str != '\0')
            {
                hash = (hash ^ static_cast<uint64>(*str)) * FnvPrime;
                ++str;
            }
            return hash;
        }

        template <typename T>
        constexpr uint64 TypeIdOf()
        {
            return Fnv1aHash(SF_RTTI_FUNC_SIG);
        }
    }

    struct TypeId
    {
        uint64 value = 0;

        constexpr TypeId() = default;
        constexpr explicit TypeId(uint64 v) : value(v) {}

        constexpr bool operator==(const TypeId &rhs) const { return value == rhs.value; }
        constexpr bool operator!=(const TypeId &rhs) const { return value != rhs.value; }
        constexpr bool operator<(const TypeId &rhs) const { return value < rhs.value; }

        constexpr bool IsValid() const { return value != 0; }
        static constexpr TypeId Null() { return TypeId(0); }
    };

    template <typename T>
    constexpr TypeId GetTypeId()
    {
        return TypeId(Detail::TypeIdOf<decay_t<T>>());
    }

    namespace Detail
    {
        template <typename T, typename = void>
        struct HasRttiImpl : false_type
        {
        };

        template <typename T>
        struct HasRttiImpl<T, void_t<decltype(T::RTTI_Type())>> : true_type
        {
        };
    }

    template <typename T>
    inline constexpr bool HasRtti = Detail::HasRttiImpl<T>::value;
}

namespace std
{
    template <>
    struct hash<SF::RTTI::TypeId>
    {
        std::size_t operator()(const SF::RTTI::TypeId &id) const noexcept
        {
            return static_cast<std::size_t>(id.value);
        }
    };
}