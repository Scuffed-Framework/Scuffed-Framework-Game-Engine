#pragma once
#include "TypeTraits.hpp" // common_type_t, make_signed_t
#include "Iterators.hpp"
#include "UsefulMacros.hpp"
#include "Containers/InitializerList.hpp"

namespace SFTL
{
    template <typename Type>
    NO_DISCARD inline constexpr auto
    begin(Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    /**
     *  @brief  Return an iterator pointing to the first element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto
    begin(const Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    /**
     *  @brief  Return an iterator pointing to one past the last element of
     *          the container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto
    end(Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    /**
     *  @brief  Return an iterator pointing to one past the last element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto
    end(const Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    /**
     *  @brief  Return an iterator pointing to the first element of the array.
     *  @param   arr  Array.
     */
    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline _GLIBCXX14_CONSTEXPR T *
    begin(T (&arr)[num]) noexcept
    {
        return arr;
    }

    /**
     *  @brief  Return an iterator pointing to one past the last element
     *          of the array.
     *  @param   arr  Array.
     */
    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline _GLIBCXX14_CONSTEXPR T *
    end(T (&arr)[num]) noexcept
    {
        return arr + num;
    }

    template <typename T>
    class valarray;
    // These overloads must be declared for cbegin and cend to use them.
    template <typename T>
    T *begin(valarray<T> &) noexcept;
    template <typename T>
    const T *begin(const valarray<T> &) noexcept;
    template <typename T>
    T *end(valarray<T> &) noexcept;
    template <typename T>
    const T *end(const valarray<T> &) noexcept;

    /**
     *  @brief  Return an iterator pointing to the first element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD constexpr auto scbegin(const Type &t) noexcept(noexcept(begin(t))) -> decltype(begin(t))
    {
        return begin(t);
    }

    /**
     *  @brief  Return an iterator pointing to one past the last element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD constexpr auto end(const Type &t) noexcept(noexcept(end(t))) -> decltype(end(t))
    {
        return end(t);
    }

    /**
     *  @brief  Return a reverse iterator pointing to the last element of
     *          the container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto rbegin(Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    /**
     *  @brief  Return a reverse iterator pointing to the last element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto rbegin(const Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    /**
     *  @brief  Return a reverse iterator pointing one past the first element of
     *          the container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto rend(Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    /**
     *  @brief  Return a reverse iterator pointing one past the first element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto rend(const Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    /**
     *  @brief  Return a reverse iterator pointing to the last element of
     *          the array.
     *  @param   arr  Array.
     */
    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr reverse_iterator<T *> rbegin(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr + num);
    }

    /**
     *  @brief  Return a reverse iterator pointing one past the first element of
     *          the array.
     *  @param   arr  Array.
     */
    template <typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr reverse_iterator<T *> rend(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr);
    }

    /**
     *  @brief  Return a reverse iterator pointing to the last element of
     *          the initializer_list.
     *  @param  list  initializer_list.
     */
    template <typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rbegin(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.end());
    }

    /**
     *  @brief  Return a reverse iterator pointing one past the first element of
     *          the initializer_list.
     *  @param  list  initializer_list.
     */
    template <typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rend(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.begin());
    }

    /**
     *  @brief  Return a reverse iterator pointing to the last element of
     *          the const container.
     *  @param  t  Container.
     */
    template <typename Type>
    NO_DISCARD inline constexpr auto crbegin(const Type &t) noexcept(noexcept(rbegin(t))) -> decltype(rbegin(t))
    {
        return rbegin(t);
    }

    /**
     *  @brief  Return a reverse iterator pointing one past the first element of
     *          the const container.
     *  @param  t  Container.
     */
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
} // namespace