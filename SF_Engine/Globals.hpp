#pragma once
#include <type_traits>
#include <vector>
namespace SF::Engine
{
    using namespace std;

    constexpr string EngineCVarNamespace = "SFE";
} // namespace SF::Engine
#if not defined NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

#if not defined DEPRECATED
    #define DEPRECATED(x) __attribute__((__deprecated__("use '" x "' instead")))
#endif
