#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <typeinfo>

#if defined(_MSC_VER)
    #define SF_RTTI_FUNC_SIG __FUNCSIG__
#elif defined(__clang__) || defined(__GNUC__)
    #define SF_RTTI_FUNC_SIG __PRETTY_FUNCTION__
#else
    #error "SF Engine RTTI requires MSVC, Clang or GCC (needs a compiler-signature macro)."
#endif

#if defined(_MSC_VER)
    #include <dbghelp.h>
    #include <windows.h>
    #pragma comment(lib, "dbghelp.lib")
#elif defined(__GNUG__)
    #include <cxxabi.h>
#endif

// for some reason this engine had 2 typeid systems prior to this ????
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

        // Runtime demangling. Only needed by GetTypeName<T>() below, for
        // types that don't carry a compile-time name via SF_RTTI /
        // SF_RTTI_BASE / SF_TYPE_INFO (those store the exact, unmangled
        // source spelling via #ClassName instead, prefer
        // RTTI_TypeName()/RTTI_GetTypeName() when a type provides them).
        // Moved here (merged in) from the old TypeInformation.hpp.
        inline std::string Demangle(const char *name)
        {
#if defined(_MSC_VER)
            char out[1024];
            if (UnDecorateSymbolName(name, out, sizeof(out), UNDNAME_COMPLETE))
                return out;
            return name;
#elif defined(__GNUG__)
            int status         = 0;
            char *demangled    = abi::__cxa_demangle(name, nullptr, nullptr, &status);
            std::string result = (status == 0 && demangled) ? demangled : name;
            free(demangled);
            return result;
#else
            return name;
#endif
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

        constexpr explicit operator size_t() const { return static_cast<size_t>(value); }

        [[nodiscard]] constexpr bool IsValid() const { return value != 0; }
        static constexpr TypeId Null() { return TypeId(0); }
    };

    // THE canonical type identity. Compile-time constant, independent of any
    // runtime registration step, and identical for a given T whether it's
    // reached through SF_RTTI macros or only ever seen generically (e.g. as
    // a template argument to a factory that never sees T's declaration).
    template<typename T>
    constexpr TypeId GetTypeId()
    {
        return TypeId(Detail::TypeIdOf<decay_t<T>>());
    }

    // Best-effort human-readable name for a type that has no compile-time
    // name of its own. Types declared with SF_RTTI/SF_RTTI_BASE/SF_TYPE_INFO
    // should prefer their own RTTI_TypeName()/RTTI_GetTypeName() instead.
    // it's the exact source spelling and doesn't need demangling.
    template<typename T>
    std::string GetTypeName()
    {
        return Detail::Demangle(typeid(decay_t<T>).name());
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
