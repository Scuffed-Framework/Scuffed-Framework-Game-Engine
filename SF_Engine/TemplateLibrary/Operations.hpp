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

    inline void *__builtin_memcpy(void *dest, const void *src, size_t n) noexcept
    {
        auto *d = static_cast<unsigned char *>(dest);
        auto const *s = static_cast<unsigned char const *>(src);

        for (size_t i = 0; i < n; ++i)
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

    template <class _Out, class _Ty>
    struct out_value_result
    {
        _Out out;
        _Ty value;

        template <_Convertible_from<const _Out &> _OOut, _Convertible_from<const _Ty &> _TTy>
        constexpr operator out_value_result<_OOut, _TTy>() const &
        {
            return {out, value};
        }

        template <_Convertible_from<_Out> _OOut, _Convertible_from<_Ty> _TTy>
        constexpr operator out_value_result<_OOut, _TTy>() &&
        {
            return {move(out), move(value)};
        }
    };

    template <class _Ty1, class _Ty2>
    concept _same_impl = is_same_v<_Ty1, _Ty2>;

    template <class _Ty1, class _Ty2>
    concept same_as = _same_impl<_Ty1, _Ty2> && _same_impl<_Ty2, _Ty1>;

    template <class _Derived, class _Base>
    concept derived_from =
        is_base_of_v<_Base, _Derived> &&
        is_convertible_v<const volatile _Derived *, const volatile _Base *>;

    template <class _From, class _To>
    concept _implicitly_convertible_to = is_convertible_v<_From, _To>;

    template <class _Ty1, class _Ty2>
    concept common_reference_with =
        requires {
            typename common_reference_t<_Ty1, _Ty2>;
            typename common_reference_t<_Ty2, _Ty1>;
        } && same_as<common_reference_t<_Ty1, _Ty2>, common_reference_t<_Ty2, _Ty1>> && convertible_to<_Ty1, common_reference_t<_Ty1, _Ty2>> && convertible_to<_Ty2, common_reference_t<_Ty1, _Ty2>>;

    template <class _LTy, class _RTy>
    concept assignable_from = is_lvalue_reference_v<_LTy> && common_reference_with<const remove_reference_t<_LTy> &, const remove_reference_t<_RTy> &> &&
                              requires(_LTy _Left, _RTy &&_Right) {
                                  { _Left = static_cast<_RTy &&>(_Right) } -> same_as<_LTy>;
                              };

    template <class _Ty>
    concept destructible = is_nothrow_destructible_v<_Ty>;

    template <class _Ty, class... _ArgTys>
    concept constructible_from = destructible<_Ty> && is_constructible_v<_Ty, _ArgTys...>;
    template <class _Ty>
    concept move_constructible = constructible_from<_Ty, _Ty> && convertible_to<_Ty, _Ty>;

    template <class _Ty>
    concept copy_constructible = move_constructible<_Ty> && constructible_from<_Ty, _Ty &> && convertible_to<_Ty &, _Ty> && constructible_from<_Ty, const _Ty &> && convertible_to<const _Ty &, _Ty> && constructible_from<_Ty, const _Ty> && convertible_to<const _Ty, _Ty>;

    template <class _Ty>
    concept _Has_class_or_enum_type =
        is_class_v<remove_reference_t<_Ty>> || is_enum_v<remove_reference_t<_Ty>> || is_union_v<remove_reference_t<_Ty>>;

    namespace ranges
    {
        namespace _swap_detail
        {
            template <class _Ty>
            void swap(_Ty &, _Ty &) = delete;

            template <class _Ty1, class _Ty2>
            concept _Use_ADL_swap =
                (_Has_class_or_enum_type<_Ty1> || _Has_class_or_enum_type<_Ty2>) && requires(_Ty1 &&__t, _Ty2 &&__u) {
                    swap(static_cast<_Ty1 &&>(__t), static_cast<_Ty2 &&>(__u)); // intentional ADL
                };

            struct _swap_FN
            {
                template <class _Ty1, class _Ty2>
                    requires _Use_ADL_swap<_Ty1, _Ty2>
                constexpr void operator()(_Ty1 &&__t, _Ty2 &&__u) const
                    noexcept(noexcept(swap(static_cast<_Ty1 &&>(__t), static_cast<_Ty2 &&>(__u))))
                {
                    swap(static_cast<_Ty1 &&>(__t), static_cast<_Ty2 &&>(__u));
                }

                template <class _Ty>
                    requires(!_Use_ADL_swap<_Ty &, _Ty &> && move_constructible<_Ty> && assignable_from<_Ty &, _Ty>)
                constexpr void operator()(_Ty &__x, _Ty &__y) const
                    noexcept(is_nothrow_move_constructible_v<_Ty> && is_nothrow_move_assignable_v<_Ty>)
                {
                    _Ty __tmp(static_cast<_Ty &&>(__x));
                    __x = static_cast<_Ty &&>(__y);
                    __y = static_cast<_Ty &&>(__tmp);
                }

                template <class _Ty1, class _Ty2, size_t _Size>
                constexpr void operator()(_Ty1 (&__t)[_Size], _Ty2 (&__u)[_Size]) const
                    noexcept(noexcept(operator()(__t[0], __u[0])))
                    requires requires(_swap_FN __fn) { __fn(__t[0], __u[0]); }
                {
                    for (size_t __i = 0; __i < _Size; ++__i)
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

    template <class _Ty>
    concept swappable = requires(_Ty &__x, _Ty &__y) { ranges::swap(__x, __y); };

    template <class _Ty>
    concept movable = is_object_v<_Ty> && move_constructible<_Ty> && assignable_from<_Ty &, _Ty> && swappable<_Ty>;

    template <class _Ty>
    concept copyable = copy_constructible<_Ty> && movable<_Ty> && assignable_from<_Ty &, _Ty &> && assignable_from<_Ty &, const _Ty &> && assignable_from<_Ty &, const _Ty>;

    template <class InputIt, class Pred>
    [[nodiscard]] constexpr InputIt find_if(InputIt first, InputIt last, Pred pred)
    {
        for (; first != last; ++first)
        {
            if (pred(*first))
                break;
        }
        return first;
    }

    template <class FwdIt, class Pred>
    constexpr FwdIt remove_if(FwdIt first, FwdIt last, Pred pred)
    {
        first = ::SFTL::find_if(first, last, pred);
        if (first == last)
            return first;

        FwdIt writePos = first;
        for (FwdIt readPos = writePos; ++readPos != last;)
        {
            if (!pred(*readPos))
            {
                *writePos = ::SFTL::move(*readPos);
                ++writePos;
            }
        }
        return writePos;
    }

    namespace detail
    {
        template <class In, class Out>
        inline constexpr bool copy_can_memmove_v =
            is_pointer_v<In> &&
            is_pointer_v<Out> &&
            is_same_v
                remove_const_t<remove_pointer_t<In>>,
                              remove_pointer_t < Out >> &&is_trivially_copyable_v<remove_pointer_t<Out>>;
    }

    template <class In, class Out>
    constexpr Out copy_unchecked(In first, In last, Out dest)
    {
        for (; first != last; ++first, (void)++dest)
            *dest = *first;
        return dest;
    }

    template <class In, class Out>
    constexpr Out copy(In first, In last, Out dest)
    {
        if constexpr (detail::copy_can_memmove_v<In, Out>)
        {
            if (!std::is_constant_evaluated())
            {
                const auto count = static_cast<size_t>(last - first);
                if (count != 0)
                    std::memmove(dest, first, count * sizeof(*dest));
                return dest + count;
            }
        }
        return copy_unchecked(first, last, dest);
    }
}