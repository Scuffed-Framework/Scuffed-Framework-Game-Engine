#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#if defined(_MSC_VER)
    #define SF_RTTI_FUNC_SIG __FUNCSIG__
#elif defined(__clang__) || defined(__GNUC__)
    #define SF_RTTI_FUNC_SIG __PRETTY_FUNCTION__
#else
    #error "SF Engine RTTI requires MSVC, Clang or GCC (needs a compiler-signature macro)."
#endif

namespace SF::RTTI
{
    using namespace std;

    namespace Detail
    {
        inline constexpr uint64_t FnvOffset = 14695981039346656037ull;
        inline constexpr uint64_t FnvPrime  = 1099511628211ull;

        // Iterative (not recursive) so it doesn't hit MSVC's constexpr
        // recursion-depth limit on long, heavily-templated signatures.
        constexpr uint64_t Fnv1aHash(const char *str)
        {
            uint64_t hash = FnvOffset;
            while (*str != '\0')
            {
                hash = (hash ^ static_cast<uint64_t>(*str)) * FnvPrime;
                ++str;
            }
            return hash;
        }

        template<typename T>
        constexpr uint64_t TypeIdOf()
        {
            return Fnv1aHash(SF_RTTI_FUNC_SIG);
        }
    } // namespace Detail

    struct TypeId
    {
        uint64_t value = 0;

        constexpr TypeId() = default;
        constexpr explicit TypeId(uint64_t v) : value(v) {}

        constexpr bool operator==(const TypeId &rhs) const { return value == rhs.value; }
        constexpr bool operator!=(const TypeId &rhs) const { return value != rhs.value; }
        constexpr bool operator<(const TypeId &rhs) const { return value < rhs.value; }

        [[nodiscard]] constexpr bool IsValid() const { return value != 0; }
        static constexpr TypeId Null() { return TypeId(0); }
    };

    template<typename T>
    constexpr TypeId GetTypeId()
    {
        return TypeId(Detail::TypeIdOf<decay_t<T>>());
    }

    namespace Detail
    {
        template<typename T, typename = void>
        struct HasRttiImpl : false_type
        {
        };

        template<typename T>
        struct HasRttiImpl<T, void_t<decltype(T::RTTI_Type())>> : true_type
        {
        };
    } // namespace Detail

    template<typename T>
    inline constexpr bool HasRtti = Detail::HasRttiImpl<T>::value;
} // namespace SF::RTTI

namespace std
{
    template<>
    struct hash<SF::RTTI::TypeId>
    {
        std::size_t operator()(const SF::RTTI::TypeId &id) const noexcept { return static_cast<std::size_t>(id.value); }
    };
} // namespace std
