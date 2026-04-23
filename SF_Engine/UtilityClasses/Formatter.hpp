#pragma once
#include <concepts>
#include <format>
#include <ranges>
#include <string>

namespace SF::Engine
{
    template <class T>
    concept HasToStringMember = requires(T a) {
        { a.to_string() } -> std::convertible_to<std::string>;
    };

    template <class T>
    concept HasToStringFree = requires(T a) {
        { to_string(a) } -> std::convertible_to<std::string>;
    };

    namespace FormatUtility
    {
        struct ToStrCPO
        {
            template <HasToStringMember T>
            auto operator()(T&& obj) const -> std::string
            {
                return obj.to_string();
            }

            template <class T>
                requires(!HasToStringMember<T> && HasToStringFree<T>)
            auto operator()(T&& obj) const -> std::string
            {
                return to_string(obj);
            }
        };

        inline constexpr ToStrCPO to_string{};
    }

    template <class T>
    struct Formatter
    {
        constexpr auto parse(std::format_parse_context& ctx)
        {
            return ctx.begin();
        }

        auto format(const T& obj, std::format_context& ctx) const
        {
            return std::format_to(ctx.out(), "{}", FormatUtility::to_string(obj));
        }
    };

    template <class T>
    concept CanFormat = requires(T a) {
        { FormatUtility::to_string(a) } -> std::convertible_to<std::string>;
    };
}

//
// Specialization must be in namespace std
//
template <SF::Engine::CanFormat T>
struct std::formatter<T> : SF::Engine::Formatter<T>
{
};
