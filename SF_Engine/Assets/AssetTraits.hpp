#pragma once
#include <filesystem>
#include <type_traits>
#include <utility>

namespace SF::Engine
{
    using namespace std;
    template<typename T, typename = void>
    struct HasWriteMember : false_type
    {
    };
    template<typename T>
    struct HasWriteMember<T, void_t<decltype(&T::Write)>> : true_type
    {
    };

    // True if T::Write is callable as Write(const Base&, const path&).
    template<typename T, typename Base, typename = void>
    struct HasValidWrite : false_type
    {
    };

    template<typename T, typename Base>
    struct HasValidWrite<T, Base,
                         void_t<decltype(T::Write(declval<const Base &>(), declval<const filesystem::path &>()))>>
        : true_type
    {
    };

} // namespace SF::Engine
