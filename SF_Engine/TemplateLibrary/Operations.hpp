/******************************************************************************/
/* Operations.hpp                                                             */
/******************************************************************************/
/*                            This file is part of                            */
/*             Scuffed Framework Standard Template Library                    */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
#pragma once
#include "TypeTraits.hpp"

namespace SFTL
{
#define SFTL_VERIFY(cond, message) \
    ((void)((cond) || (printf(message), 0)))

    inline void *SFTL_BI_MCPY(void *dest, const void *src, size_type n) noexcept
    {
        auto *d = static_cast<unsigned char *>(dest);
        auto const *s = static_cast<unsigned char const *>(src);

        for (size_type i = 0; i < n; ++i)
            d[i] = s[i];

        return dest;
    }

    template <class _To, class _From>
    concept _Convertible_from = is_convertible_v<_From, _To>;

    template <class _In, class _Out>
    struct in_out_result
    {
        _In in;
        _Out out;

        template <_Convertible_from<const _In &> _IIn, _Convertible_from<const _Out &> _OOut>
        constexpr operator in_out_result<_IIn, _OOut>() const &
        {
            return {in, out};
        }

        template <_Convertible_from<_In> _IIn, _Convertible_from<_Out> _OOut>
        constexpr operator in_out_result<_IIn, _OOut>() &&
        {
            return {move(in), move(out)};
        }
    };

    template <class _Out, class Type>
    struct out_value_result
    {
        _Out out;
        Type value;

        template <_Convertible_from<const _Out &> _OOut, _Convertible_from<const Type &> _TTy>
        constexpr operator out_value_result<_OOut, _TTy>() const &
        {
            return {out, value};
        }

        template <_Convertible_from<_Out> _OOut, _Convertible_from<Type> _TTy>
        constexpr operator out_value_result<_OOut, _TTy>() &&
        {
            return {move(out), move(value)};
        }
    };

    template <class Type1, class Type2>
    concept _same_impl = is_same_v<Type1, Type2>;

    template <class Type1, class Type2>
    concept same_as = _same_impl<Type1, Type2> && _same_impl<Type2, Type1>;

    template <class _Derived, class _Base>
    concept derived_from =
        is_base_of_v<_Base, _Derived> &&
        is_convertible_v<const volatile _Derived *, const volatile _Base *>;

    template <class _From, class _To>
    concept _implicitly_convertible_to = is_convertible_v<_From, _To>;

    template <class Type1, class Type2>
    concept common_reference_with =
        requires {
            typename common_reference_t<Type1, Type2>;
            typename common_reference_t<Type2, Type1>;
        } && same_as<common_reference_t<Type1, Type2>, common_reference_t<Type2, Type1>> && convertible_to<Type1, common_reference_t<Type1, Type2>> && convertible_to<Type2, common_reference_t<Type1, Type2>>;

    template <class _LTy, class _RTy>
    concept assignable_from = is_lvalue_reference_v<_LTy> && common_reference_with<const remove_reference_t<_LTy> &, const remove_reference_t<_RTy> &> &&
                              requires(_LTy _Left, _RTy &&_Right) {
                                  { _Left = static_cast<_RTy &&>(_Right) } -> same_as<_LTy>;
                              };

    template <class Type>
    concept destructible = is_nothrow_destructible_v<Type>;

    template <class Type, class... _ArgTys>
    concept constructible_from = destructible<Type> && is_constructible_v<Type, _ArgTys...>;
    template <class Type>
    concept move_constructible = constructible_from<Type, Type> && convertible_to<Type, Type>;

    template <class Type>
    concept copy_constructible = move_constructible<Type> && constructible_from<Type, Type &> && convertible_to<Type &, Type> && constructible_from<Type, const Type &> && convertible_to<const Type &, Type> && constructible_from<Type, const Type> && convertible_to<const Type, Type>;

    template <class Type>
    concept _Has_class_or_enum_type =
        is_class_v<remove_reference_t<Type>> || is_enum_v<remove_reference_t<Type>> || is_union_v<remove_reference_t<Type>>;

    namespace ranges
    {
        namespace _swap_detail
        {
            template <class Type>
            void swap(Type &, Type &) = delete;

            template <class Type1, class Type2>
            concept _Use_ADL_swap =
                (_Has_class_or_enum_type<Type1> || _Has_class_or_enum_type<Type2>) && requires(Type1 &&__t, Type2 &&__u) {
                    swap(static_cast<Type1 &&>(__t), static_cast<Type2 &&>(__u)); // intentional ADL
                };

            struct _swap_FN
            {
                template <class Type1, class Type2>
                    requires _Use_ADL_swap<Type1, Type2>
                constexpr void operator()(Type1 &&__t, Type2 &&__u) const
                    noexcept(noexcept(swap(static_cast<Type1 &&>(__t), static_cast<Type2 &&>(__u))))
                {
                    swap(static_cast<Type1 &&>(__t), static_cast<Type2 &&>(__u));
                }

                template <class Type>
                    requires(!_Use_ADL_swap<Type &, Type &> && move_constructible<Type> && assignable_from<Type &, Type>)
                constexpr void operator()(Type &__x, Type &__y) const
                    noexcept(is_nothrow_move_constructible_v<Type> && is_nothrow_move_assignable_v<Type>)
                {
                    Type __tmp(static_cast<Type &&>(__x));
                    __x = static_cast<Type &&>(__y);
                    __y = static_cast<Type &&>(__tmp);
                }

                template <class Type1, class Type2, size_type sz>
                constexpr void operator()(Type1 (&__t)[sz], Type2 (&__u)[sz]) const
                    noexcept(noexcept(operator()(__t[0], __u[0])))
                    requires requires(_swap_FN __fn) { __fn(__t[0], __u[0]); }
                {
                    for (size_type __i = 0; __i < sz; ++__i)
                    {
                        operator()(__t[__i], __u[__i]);
                    }
                }
            };
        }

        inline namespace _Cpos
        {
            inline constexpr _swap_detail::_swap_FN swap;
        }
    } // namespace ranges

    template <class Type>
    concept swappable = requires(Type &__x, Type &__y) { ranges::swap(__x, __y); };

    template <class Type>
    concept movable = is_object_v<Type> && move_constructible<Type> && assignable_from<Type &, Type> && swappable<Type>;

    template <class Type>
    concept copyable = copy_constructible<Type> && movable<Type> && assignable_from<Type &, Type &> && assignable_from<Type &, const Type &> && assignable_from<Type &, const Type>;

    template <class InputIterator, class Predicate>
    [[nodiscard]] constexpr InputIterator find_if(InputIterator first, InputIterator last, Predicate predicate)
    {
        for (; first != last; ++first)
        {
            if (predicate(*first))
                break;
        }
        return first;
    }

    template <class ForwardIterator, class Predicate>
    [[nodiscard]] constexpr ForwardIterator remove_if(ForwardIterator first, ForwardIterator last, Predicate predicate)
    {
        // Skip the leading run that doesn't match; nothing to compact yet.
        first = ::SFTL::find_if(first, last, predicate);
        if (first == last)
            return first;

        // 'first' now points at the first element to be removed.
        // Slide every subsequent non-matching element back into place.
        ForwardIterator dest = first;
        for (ForwardIterator it = first; ++it != last;)
        {
            if (!predicate(*it))
                *dest++ = ::SFTL::move(*it);
        }

        return dest;
    }

    namespace detail
    {
        template <class In, class Out>
        inline constexpr bool copy_can_memmove_v =
            is_pointer_v<In> &&
            is_pointer_v<Out> &&
            is_same_v<remove_const_t<remove_pointer_t<In>>, remove_pointer_t<Out>> &&
            is_trivially_copyable_v<remove_pointer_t<Out>>;
    }

    template <class In, class Out>
    constexpr Out copy_unchecked(In first, In last, Out dest)
    {
        for (; first != last; ++first, (void)++dest)
            *dest = *first;
        return dest;
    }

    inline void *SFTL_memmove(void *dest, const void *src, size_type count)
    {
        unsigned char *d = static_cast<unsigned char *>(dest);
        const unsigned char *s = static_cast<const unsigned char *>(src);

        if (d == s || count == 0)
        {
            return dest;
        }

        if (d < s)
        {
            // Copy forward
            for (size_type i = 0; i < count; ++i)
            {
                d[i] = s[i];
            }
        }
        else
        {
            // Copy backward to prevent overwrite
            for (size_type i = count; i > 0; --i)
            {
                d[i - 1] = s[i - 1];
            }
        }

        return dest;
    }

    template <class In, class Out>
    constexpr Out copy(In first, In last, Out dest)
    {
        if constexpr (detail::copy_can_memmove_v<In, Out>)
        {
            if (!std::is_constant_evaluated())
            {
                const auto count = static_cast<size_type>(last - first);
                if (count != 0)
                    SFTL_memmove(dest, first, count * sizeof(*dest));
                return dest + count;
            }
        }
        return copy_unchecked(first, last, dest);
    }
}