#pragma once
#include "TypeTraits.hpp"
#include "Iterators.hpp"
#include "UsefulMacros.hpp"
#include "Containers/InitializerList.hpp"

namespace SFTL
{
    template <typename Type>
    NO_DISCARD inline constexpr auto begin(Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto begin(const Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto end(Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto end(const Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr T *begin(T (&arr)[num]) noexcept
    {
        return arr;
    }

    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr T *
    end(T (&arr)[num]) noexcept
    {
        return arr + num;
    }

    template <typename Type>
    NO_DISCARD constexpr auto scbegin(const Type &t) noexcept(noexcept(begin(t))) -> decltype(begin(t))
    {
        return begin(t);
    }

    template <typename Type>
    NO_DISCARD constexpr auto end(const Type &t) noexcept(noexcept(end(t))) -> decltype(end(t))
    {
        return end(t);
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto rbegin(Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto rbegin(const Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto rend(Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto rend(const Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr reverse_iterator<T *> rbegin(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr + num);
    }

    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr reverse_iterator<T *> rend(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr);
    }

    template <typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rbegin(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.end());
    }

    template <typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rend(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.begin());
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto crbegin(const Type &t) noexcept(noexcept(rbegin(t))) -> decltype(rbegin(t))
    {
        return rbegin(t);
    }

    template <typename Type>
    NO_DISCARD inline constexpr auto crend(const Type &t) noexcept(noexcept(rend(t))) -> decltype(rend(t))
    {
        return rend(t);
    }

    template <typename T, ptrdiff_t v>
    NO_DISCARD constexpr ptrdiff_t ssize(const T (&)[v]) noexcept
    {
        return v;
    }
} // namespace SFTL
