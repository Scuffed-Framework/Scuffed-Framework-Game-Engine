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
#define SFTL_VERIFY(cond, message) ((void) ((cond) || (printf(message), 0)))

namespace SFTL
{

    inline void *SFTL_BI_MCPY(void *dest, const void *src, size_type n) noexcept
    {
        auto *d       = static_cast<unsigned char *>(dest);
        auto const *s = static_cast<unsigned char const *>(src);

        for (size_type i = 0; i < n; ++i)
            d[i] = s[i];

        return dest;
    }

    template<class To, class From>
    concept ConvertibleFrom = is_convertible_v<From, To>;

    template<class Input, class Output>
    struct in_out_result
    {
        Input in;
        Output out;

        template<ConvertibleFrom<const Input &> input, ConvertibleFrom<const Output &> output>
        explicit constexpr operator in_out_result<input, output>() const &
        {
            return {in, out};
        }

        template<ConvertibleFrom<Input> input, ConvertibleFrom<Output> output>
        constexpr operator in_out_result<input, output>() &&
        {
            return {move(in), move(out)};
        }
    };

    template<class Output, class Type>
    struct out_value_result
    {
        Output out;
        Type value;

        template<ConvertibleFrom<const Output &> output, ConvertibleFrom<const Type &> cvType>
        explicit constexpr operator out_value_result<output, cvType>() const &
        {
            return {out, value};
        }

        template<ConvertibleFrom<Output> output, ConvertibleFrom<Type> cvType>
        explicit constexpr operator out_value_result<output, cvType>() &&
        {
            return {move(out), move(value)};
        }
    };

    template<class Type1, class Type2>
    concept _same_impl = is_same_v<Type1, Type2>;

    template<class Type1, class Type2>
    concept same_as = _same_impl<Type1, Type2> && _same_impl<Type2, Type1>;

    template<class Derived, class Base>
    concept derived_from =
            is_base_of_v<Base, Derived> && is_convertible_v<const volatile Derived *, const volatile Base *>;

    template<class From, class To>
    concept _implicitly_convertible_to = is_convertible_v<From, To>;

    template<class Type1, class Type2>
    concept common_reference_with =
            requires {
                typename common_reference_t<Type1, Type2>;
                typename common_reference_t<Type2, Type1>;
            } && same_as<common_reference_t<Type1, Type2>, common_reference_t<Type2, Type1>> &&
            convertible_to<Type1, common_reference_t<Type1, Type2>> &&
            convertible_to<Type2, common_reference_t<Type1, Type2>>;

    template<class LeftType, class RightType>
    concept assignable_from =
            is_lvalue_reference_v<LeftType> &&
            common_reference_with<const remove_reference_t<LeftType> &, const remove_reference_t<RightType> &> &&
            requires(LeftType left, RightType &&right) {
                { left = static_cast<RightType &&>(right) } -> same_as<LeftType>;
            };

    template<class Type>
    concept destructible = is_nothrow_destructible_v<Type>;

    template<class Type, class... ArgumentType>
    concept constructible_from = destructible<Type> && is_constructible_v<Type, ArgumentType...>;
    template<class Type>
    concept move_constructible = constructible_from<Type, Type> && convertible_to<Type, Type>;

    template<class Type>
    concept copy_constructible =
            move_constructible<Type> && constructible_from<Type, Type &> && convertible_to<Type &, Type> &&
            constructible_from<Type, const Type &> && convertible_to<const Type &, Type> &&
            constructible_from<Type, const Type> && convertible_to<const Type, Type>;

    template<class Type>
    concept HasClassOrEnumType = is_class_v<remove_reference_t<Type>> || is_enum_v<remove_reference_t<Type>> ||
                                 is_union_v<remove_reference_t<Type>>;

    namespace ranges
    {
        namespace swap_detail
        {
            template<class Type>
            void swap(Type &, Type &) noexcept = delete;

            template<class Type1, class Type2>
            concept UseADLSwap =
                    (HasClassOrEnumType<Type1> || HasClassOrEnumType<Type2>) && requires(Type1 &&type1, Type2 &&type2) {
                        swap(static_cast<Type1 &&>(type1), static_cast<Type2 &&>(type2)); // intentional ADL
                    };

            struct SwapFunction
            {
                template<class Type1, class Type2>
                    requires UseADLSwap<Type1, Type2>
                constexpr void operator()(Type1 &&type1, Type2 &&type2) const
                        noexcept(noexcept(swap(static_cast<Type1 &&>(type1), static_cast<Type2 &&>(type2))))
                {
                    swap(static_cast<Type1 &&>(type1), static_cast<Type2 &&>(type2));
                }

                template<class Type>
                    requires(!UseADLSwap<Type &, Type &> && move_constructible<Type> && assignable_from<Type &, Type>)
                constexpr void operator()(Type &x, Type &y) const
                        noexcept(is_nothrow_move_constructible_v<Type> && is_nothrow_move_assignable_v<Type>)
                {
                    Type type1mp(static_cast<Type &&>(x));
                    x = static_cast<Type &&>(y);
                    y = static_cast<Type &&>(type1mp);
                }

                template<class Type1, class Type2, size_type sz>
                constexpr void operator()(Type1 (&type1)[sz], Type2 (&type2)[sz]) const
                        noexcept(noexcept(operator()(type1[0], type2[0])))
                    requires requires { declval<const SwapFunction &>()(declval<Type1 &>(), declval<Type2 &>()); }
                {
                    for (size_type st = 0; st < sz; ++st)
                    {
                        operator()(type1[st], type2[st]);
                    }
                }
            };
        } // namespace swap_detail

        inline namespace in
        {
            inline constexpr swap_detail::SwapFunction swap;
        }
    } // namespace ranges

    template<class Type>
    concept swappable = requires(Type &x, Type &y) { ranges::swap(x, y); };

    template<class Type>
    concept movable = is_object_v<Type> && move_constructible<Type> && assignable_from<Type &, Type> && swappable<Type>;

    template<class Type>
    concept copyable = copy_constructible<Type> && movable<Type> && assignable_from<Type &, Type &> &&
                       assignable_from<Type &, const Type &> && assignable_from<Type &, const Type>;

    template<class InputIterator, class Predicate>
    [[nodiscard]] constexpr InputIterator find_if(InputIterator first, InputIterator last, Predicate predicate)
    {
        for (; first != last; ++first)
        {
            if (predicate(*first))
                break;
        }
        return first;
    }

    template<class ForwardIterator, class Predicate>
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

    template<typename InputIterator, typename Type>
    InputIterator find(InputIterator first, InputIterator last, const Type &value)
    {
        return find_if(first, last, equal_to<Type>(value));
    }

    namespace detail
    {
        template<class In, class Out>
        inline constexpr bool copy_can_memmove_v =
                is_pointer_v<In> && is_pointer_v<Out> &&
                is_same_v<remove_const_t<remove_pointer_t<In>>, remove_pointer_t<Out>> &&
                is_trivially_copyable_v<remove_pointer_t<Out>>;
    }

    template<class In, class Out>
    constexpr Out copy_unchecked(In first, In last, Out dest)
    {
        for (; first != last; ++first, (void) ++dest)
            *dest = *first;
        return dest;
    }

    inline void *SFTL_memmove(void *dest, const void *src, size_type count)
    {
        auto *d       = static_cast<unsigned char *>(dest);
        const auto *s = static_cast<const unsigned char *>(src);

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
        } else
        {
            // Copy backward to prevent overwrite
            for (size_type i = count; i > 0; --i)
            {
                d[i - 1] = s[i - 1];
            }
        }

        return dest;
    }

    template<class In, class Out>
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

    using std::addressof;

    template<typename Type>
    constexpr inline void destroy_at(Type *Location)
    {
        if constexpr (__cplusplus > 201703L && is_array_v<Type>)
        {
            for (auto &x: *Location)
                destroy_at(addressof(x));
        } else
            Location->~Type();
    }

    template<typename Type, typename... Arguments>
        requires(!is_unbounded_array_v<Type>) && requires { ::new ((void *) nullptr) Type(declval<Arguments>()...); }
    constexpr Type *construct_at(Type *Location,
                                 Arguments &&...args) noexcept(noexcept(::new ((void *) nullptr)
                                                                                Type(std::declval<Arguments>()...)))
    {
        void *loc = Location;
        if constexpr (is_array_v<Type>)
        {
            static_assert(sizeof...(Arguments) == 0, "SFTL::construct_at for array "
                                                     "types must not use any arguments to initialize the "
                                                     "array");
            return ::new (loc) Type[1]();
        } else
            return ::new (loc) Type(forward<Arguments>(args)...);
    }

    template<typename Type, typename... Argument>
    constexpr inline void Construct(Type *type, Argument &&...arguments)
    {
        if (is_constant_evaluated())
        {
            construct_at(type, forward<Argument>(arguments)...);
            return;
        }
        ::new (static_cast<void *>(type)) Type(forward<Argument>(arguments)...);
    }
} // namespace SFTL
