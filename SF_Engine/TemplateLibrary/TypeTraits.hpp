/******************************************************************************/
/* TypeTraits.hpp                                                             */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin (the name I was assigned in french class).                                         */
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

namespace SFTL
{

    // =============================================================================
    // Internal helpers (not for direct use)
    // =============================================================================

    namespace detail
    {
        // Separate lvalue/rvalue overloads so declval<void>() still compiles
        template <typename T>
        T &&declval_impl(int) noexcept;
        template <typename T>
        T declval_impl(long) noexcept;
    } // namespace detail

    // =============================================================================
    // declval
    // =============================================================================

    /// Produces a value of type T in an unevaluated context without construction.
    /// Mirrors std::declval. Never defined; ODR-safe because it is never called.
    template <typename T>
    auto declval() noexcept -> decltype(detail::declval_impl<T>(0));

    // =============================================================================
    // integral_constant / bool_constant / true_type / false_type
    // =============================================================================

    template <typename T, T val>
    struct integral_constant
    {
        static constexpr T value = val;
        using value_type = T;
        using type = integral_constant<T, val>;

        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    template <bool b>
    using bool_constant = integral_constant<bool, b>;

    using true_type = bool_constant<true>;
    using false_type = bool_constant<false>;

    // =============================================================================
    // type_identity  (useful building block  gives back T unchanged)
    // =============================================================================

    template <typename T>
    struct type_identity
    {
        using type = T;
    };

    template <typename T>
    using type_identity_t = typename type_identity<T>::type;

    // =============================================================================
    // conditional
    // =============================================================================

    template <bool Cond, typename IfTrue, typename IfFalse>
    struct conditional
    {
        using type = IfTrue;
    };

    template <typename IfTrue, typename IfFalse>
    struct conditional<false, IfTrue, IfFalse>
    {
        using type = IfFalse;
    };

    template <bool Cond, typename IfTrue, typename IfFalse>
    using conditional_t = typename conditional<Cond, IfTrue, IfFalse>::type;

    // =============================================================================
    // enable_if
    // =============================================================================

    template <bool Cond, typename T = void>
    struct enable_if
    {
    };

    template <typename T>
    struct enable_if<true, T>
    {
        using type = T;
    };

    template <bool Cond, typename T = void>
    using enable_if_t = typename enable_if<Cond, T>::type;

    // =============================================================================
    // void_t  (detection idiom helper)
    // =============================================================================

    template <typename...>
    using void_t = void;

    // =============================================================================
    // remove / add qualifiers
    // =============================================================================

    // --- const ---
    template <typename T>
    struct remove_const
    {
        using type = T;
    };
    template <typename T>
    struct remove_const<const T>
    {
        using type = T;
    };
    template <typename T>
    using remove_const_t = typename remove_const<T>::type;

    // --- volatile ---
    template <typename T>
    struct remove_volatile
    {
        using type = T;
    };
    template <typename T>
    struct remove_volatile<volatile T>
    {
        using type = T;
    };
    template <typename T>
    using remove_volatile_t = typename remove_volatile<T>::type;

    // --- cv ---
    template <typename T>
    struct remove_cv
    {
        using type = remove_volatile_t<remove_const_t<T>>;
    };
    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    // --- reference ---
    template <typename T>
    struct remove_reference
    {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T &>
    {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T &&>
    {
        using type = T;
    };
    template <typename T>
    using remove_reference_t = typename remove_reference<T>::type;

    // --- cvref ---
    template <typename T>
    struct remove_cvref
    {
        using type = remove_cv_t<remove_reference_t<T>>;
    };
    template <typename T>
    using remove_cvref_t = typename remove_cvref<T>::type;

    // --- pointer ---
    template <typename T>
    struct remove_pointer
    {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T *>
    {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T *const>
    {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T *volatile>
    {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T *const volatile>
    {
        using type = T;
    };
    template <typename T>
    using remove_pointer_t = typename remove_pointer<T>::type;

    // --- add_lvalue_reference / add_rvalue_reference ---
    namespace detail
    {
        template <typename T, typename = void>
        struct add_lref
        {
            using type = T;
        };
        template <typename T>
        struct add_lref<T, void_t<T &>>
        {
            using type = T &;
        };

        template <typename T, typename = void>
        struct add_rref
        {
            using type = T;
        };
        template <typename T>
        struct add_rref<T, void_t<T &&>>
        {
            using type = T &&;
        };
    } // namespace detail

    template <typename T>
    struct add_lvalue_reference : detail::add_lref<T>
    {
    };
    template <typename T>
    using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

    template <typename T>
    struct add_rvalue_reference : detail::add_rref<T>
    {
    };
    template <typename T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

    // =============================================================================
    // is_same
    // =============================================================================

    template <typename T, typename U>
    struct is_same : false_type
    {
    };
    template <typename T>
    struct is_same<T, T> : true_type
    {
    };

    template <typename T, typename U>
    inline constexpr bool is_same_v = is_same<T, U>::value;

    // =============================================================================
    // is_void
    // =============================================================================

    template <typename T>
    struct is_void : is_same<void, remove_cv_t<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_void_v = is_void<T>::value;

    // =============================================================================
    // is_const / is_volatile / is_reference / is_pointer
    // =============================================================================

    template <typename T>
    struct is_const : false_type
    {
    };
    template <typename T>
    struct is_const<const T> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_const_v = is_const<T>::value;

    template <typename T>
    struct is_volatile : false_type
    {
    };
    template <typename T>
    struct is_volatile<volatile T> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_volatile_v = is_volatile<T>::value;

    template <typename T>
    struct is_lvalue_reference : false_type
    {
    };
    template <typename T>
    struct is_lvalue_reference<T &> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

    template <typename T>
    struct is_rvalue_reference : false_type
    {
    };
    template <typename T>
    struct is_rvalue_reference<T &&> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

    template <typename T>
    struct is_reference : bool_constant<is_lvalue_reference_v<T> || is_rvalue_reference_v<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_reference_v = is_reference<T>::value;

    template <typename T>
    struct is_pointer : false_type
    {
    };
    template <typename T>
    struct is_pointer<T *> : true_type
    {
    };
    template <typename T>
    struct is_pointer<T *const> : true_type
    {
    };
    template <typename T>
    struct is_pointer<T *volatile> : true_type
    {
    };
    template <typename T>
    struct is_pointer<T *const volatile> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_pointer_v = is_pointer<T>::value;

    // =============================================================================
    // is_array
    // =============================================================================

    template <typename T>
    struct is_array : false_type
    {
    };
    template <typename T>
    struct is_array<T[]> : true_type
    {
    };
    template <typename T, size_t N>
    struct is_array<T[N]> : true_type
    {
    };
    template <typename T>
    inline constexpr bool is_array_v = is_array<T>::value;

    // =============================================================================
    // is_integral / is_floating_point / is_arithmetic
    // =============================================================================

    namespace detail
    {
        template <typename T>
        struct is_integral_base : false_type
        {
        };
        template <>
        struct is_integral_base<bool> : true_type
        {
        };
        template <>
        struct is_integral_base<char> : true_type
        {
        };
        template <>
        struct is_integral_base<signed char> : true_type
        {
        };
        template <>
        struct is_integral_base<unsigned char> : true_type
        {
        };
        template <>
        struct is_integral_base<wchar_t> : true_type
        {
        };
        template <>
        struct is_integral_base<char8_t> : true_type
        {
        };
        template <>
        struct is_integral_base<char16_t> : true_type
        {
        };
        template <>
        struct is_integral_base<char32_t> : true_type
        {
        };
        template <>
        struct is_integral_base<short> : true_type
        {
        };
        template <>
        struct is_integral_base<unsigned short> : true_type
        {
        };
        template <>
        struct is_integral_base<int> : true_type
        {
        };
        template <>
        struct is_integral_base<unsigned int> : true_type
        {
        };
        template <>
        struct is_integral_base<long> : true_type
        {
        };
        template <>
        struct is_integral_base<unsigned long> : true_type
        {
        };
        template <>
        struct is_integral_base<long long> : true_type
        {
        };
        template <>
        struct is_integral_base<unsigned long long> : true_type
        {
        };
    } // namespace detail

    template <typename T>
    struct is_integral : detail::is_integral_base<remove_cv_t<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_integral_v = is_integral<T>::value;

    namespace detail
    {
        template <typename T>
        struct is_fp_base : false_type
        {
        };
        template <>
        struct is_fp_base<float> : true_type
        {
        };
        template <>
        struct is_fp_base<double> : true_type
        {
        };
        template <>
        struct is_fp_base<long double> : true_type
        {
        };
    } // namespace detail

    template <typename T>
    struct is_floating_point : detail::is_fp_base<remove_cv_t<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

    template <typename T>
    struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

    // =============================================================================
    // is_signed / is_unsigned  (arithmetic types only)
    // =============================================================================

    template <typename T, bool = is_arithmetic_v<T>>
    struct is_signed : bool_constant<(T(-1) < T(0))>
    {
    };
    template <typename T>
    struct is_signed<T, false> : false_type
    {
    };
    template <typename T>
    inline constexpr bool is_signed_v = is_signed<T>::value;

    template <typename T>
    struct is_unsigned : bool_constant<is_arithmetic_v<T> && !is_signed_v<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

    // =============================================================================
    // is_enum / is_union / is_class
    // (These three require compiler builtins; there is no portable TMP alternative)
    // =============================================================================

    template <typename T>
    struct is_enum : bool_constant<__is_enum(T)>
    {
    };
    template <typename T>
    inline constexpr bool is_enum_v = is_enum<T>::value;

    template <typename T>
    struct is_union : bool_constant<__is_union(T)>
    {
    };
    template <typename T>
    inline constexpr bool is_union_v = is_union<T>::value;

    template <typename T>
    struct is_class : bool_constant<__is_class(T)>
    {
    };
    template <typename T>
    inline constexpr bool is_class_v = is_class<T>::value;

    // =============================================================================
    // is_base_of
    // =============================================================================

    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)>
    {
    };
    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

    // =============================================================================
    // is_convertible
    // =============================================================================

    namespace detail
    {
        template <typename To>
        void test_convertible(To) noexcept;

        template <typename From, typename To, typename = void>
        struct is_convertible_impl : false_type
        {
        };

        template <typename From, typename To>
        struct is_convertible_impl<From, To,
                                   void_t<decltype(test_convertible<To>(declval<From>()))>> : true_type
        {
        };
    } // namespace detail

    template <typename From, typename To>
    struct is_convertible : detail::is_convertible_impl<From, To>
    {
    };
    template <typename From, typename To>
    inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

    // =============================================================================
    // is_constructible / is_default_constructible / is_copy_constructible
    // =============================================================================

    namespace detail
    {
        template <typename T, typename... Args>
        struct is_constructible_impl
        {
        private:
            template <typename U, typename... As>
            static auto test(int) -> decltype(void(U(declval<As>()...)), true_type{});
            template <typename, typename...>
            static false_type test(...);

        public:
            using type = decltype(test<T, Args...>(0));
        };
    } // namespace detail

    template <typename T, typename... Args>
    struct is_constructible : detail::is_constructible_impl<T, Args...>::type
    {
    };
    template <typename T, typename... Args>
    inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

    template <typename T>
    struct is_default_constructible : is_constructible<T>
    {
    };
    template <typename T>
    inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

    template <typename T>
    struct is_copy_constructible : is_constructible<T, add_lvalue_reference_t<const T>>
    {
    };
    template <typename T>
    inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

    template <typename T>
    struct is_move_constructible : is_constructible<T, add_rvalue_reference_t<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

    // =============================================================================
    // is_assignable / is_copy_assignable / is_move_assignable
    // =============================================================================

    namespace detail
    {
        template <typename T, typename U, typename = void>
        struct is_assignable_impl : false_type
        {
        };
        template <typename T, typename U>
        struct is_assignable_impl<T, U,
                                  void_t<decltype(declval<T>() = declval<U>())>> : true_type
        {
        };
    } // namespace detail

    template <typename T, typename U>
    struct is_assignable : detail::is_assignable_impl<T, U>
    {
    };
    template <typename T, typename U>
    inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

    template <typename T>
    struct is_copy_assignable
        : is_assignable<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>>
    {
    };
    template <typename T>
    inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

    template <typename T>
    struct is_move_assignable
        : is_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

    // =============================================================================
    // Nothrow variants  (your originals, fixed)
    // =============================================================================

    template <typename T>
    struct is_nothrow_move_constructible
        : bool_constant<noexcept(T(declval<T &&>()))>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_move_constructible_v =
        is_nothrow_move_constructible<T>::value;

    template <typename T>
    struct is_nothrow_move_assignable
        : bool_constant<noexcept(declval<T &>() = declval<T &&>())>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v =
        is_nothrow_move_assignable<T>::value;

    template <typename T>
    struct is_nothrow_copy_constructible
        : bool_constant<noexcept(T(declval<const T &>()))>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_copy_constructible_v =
        is_nothrow_copy_constructible<T>::value;

    template <typename T>
    struct is_nothrow_copy_assignable
        : bool_constant<noexcept(declval<T &>() = declval<const T &>())>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_copy_assignable_v =
        is_nothrow_copy_assignable<T>::value;

    template <typename T>
    struct is_nothrow_default_constructible
        : bool_constant<noexcept(T())>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_default_constructible_v =
        is_nothrow_default_constructible<T>::value;

    // =============================================================================
    // decay
    // =============================================================================

    // remove_extent (needed by decay)
    template <typename T>
    struct remove_extent
    {
        using type = T;
    };
    template <typename T>
    struct remove_extent<T[]>
    {
        using type = T;
    };
    template <typename T, size_t N>
    struct remove_extent<T[N]>
    {
        using type = T;
    };
    template <typename T>
    using remove_extent_t = typename remove_extent<T>::type;

    /*
    namespace detail
    {
        template <typename T>
        struct decay_impl
        {
        private:
            using U = remove_reference_t<T>;

        public:
            using type = conditional_t
                is_array_v<U>,
                  remove_extent_t<U> *,
                  remove_cv_t<U> > ;
        };
    } // namespace detail

    template <typename T>
    struct decay : detail::decay_impl<T>
    {
    };
    template <typename T>
    using decay_t = typename decay<T>::type;
    */
    // =============================================================================
    // move / forward  (implementation only  normally in <utility>)
    // =============================================================================

    template <typename T>
    constexpr remove_reference_t<T> &&move(T &&t) noexcept
    {
        return static_cast<remove_reference_t<T> &&>(t);
    }

    template <typename T>
    constexpr T &&forward(remove_reference_t<T> &t) noexcept
    {
        return static_cast<T &&>(t);
    }

    template <typename T>
    constexpr T &&forward(remove_reference_t<T> &&t) noexcept
    {
        static_assert(!is_lvalue_reference_v<T>,
                      "SFTL::forward: cannot forward an rvalue as an lvalue");
        return static_cast<T &&>(t);
    }

    // =============================================================================
    // swap
    // =============================================================================

    template <typename T>
    constexpr enable_if_t<is_move_constructible_v<T> && is_move_assignable_v<T>>
    swap(T &a, T &b) noexcept(is_nothrow_move_constructible_v<T> &&
                              is_nothrow_move_assignable_v<T>)
    {
        T tmp = move(a);
        a = move(b);
        b = move(tmp);
    }

} // namespace SFTL