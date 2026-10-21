#pragma once
#include <type_traits>
#include <vector>
namespace SF::Engine
{
    using namespace std;

    constexpr string EngineCVarNamespace = "SFE";

    template<typename T>
    using AdvancedArray = vector<T>;

    template<typename T>
    constexpr T &&LToR(remove_reference_t<T> &t) noexcept
    {
        return static_cast<T &&>(t);
    }

    template<typename T>
    constexpr T &&RToL(remove_reference_t<T> &&t) noexcept
    {
        static_assert(!std::is_lvalue_reference_v<T>, "Cannot convert rvalue to lvalue as an lvalue.");

        return static_cast<T &&>(t);
    }
} // namespace SF::Engine
#if not defined NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

#if not defined DEPRECATED
    #define DEPRECATED(x) __attribute__((__deprecated__("use '" x "' instead")))
#endif
