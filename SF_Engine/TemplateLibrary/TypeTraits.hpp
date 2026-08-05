/******************************************************************************/
/* TypeTraits.hpp                                                             */
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
#include <type_traits> // intrinsics 🥀

namespace SFTL
{
    namespace detail
    {
        template <typename T>
        T &&declval_impl(int) noexcept;
        template <typename T>
        T declval_impl(long) noexcept;
    } // namespace detail

    template <typename T>
    auto declval() noexcept -> decltype(detail::declval_impl<T>(0));

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

    // type_identity  (useful building block  gives back T unchanged)

    template <typename T>
    struct type_identity
    {
        using type = T;
    };

    template <typename T>
    using type_identity_t = typename type_identity<T>::type;

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

    // enable_if

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

    template <typename...>
    using void_t = void;

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

    template <typename T>
    struct remove_cv
    {
        using type = remove_volatile_t<remove_const_t<T>>;
    };
    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

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

    template <typename T>
    struct remove_cvref
    {
        using type = remove_cv_t<remove_reference_t<T>>;
    };
    template <typename T>
    using remove_cvref_t = typename remove_cvref<T>::type;

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

    template <typename T>
    struct is_void : is_same<void, remove_cv_t<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_void_v = is_void<T>::value;

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

    // is_function: the classic trick  functions are the only types for which
    // "const T" does not actually add const, and which are also not references.
    template <typename T>
    struct is_function : bool_constant<!is_const_v<const T> && !is_reference_v<T>>
    {
    };
    template <typename T>
    inline constexpr bool is_function_v = is_function<T>::value;

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

    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)>
    {
    };
    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

    namespace detail
    {
        template <typename To>
        void test_convertible(To) noexcept;

        template <typename From, typename To, typename = void>
        struct is_convertible_impl : false_type
        {
        };

        template <typename From, typename To>
        struct is_convertible_impl<From, To, void_t<decltype(test_convertible<To>(declval<From>()))>> : true_type
        {
        };
    } // namespace detail

    template <typename From, typename To>
    struct is_convertible : detail::is_convertible_impl<From, To>
    {
    };
    template <typename From, typename To>
    inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

    template <typename From, typename To>
    class is_convertible_to_impl
    {
    private:
        static void test(To);

        template <typename F, typename = decltype(test(declval<F>()))>
        static true_type check(int);

        template <typename>
        static false_type check(...);

    public:
        static constexpr bool value = decltype(check<From>(0))::value;
    };

    template <typename From, typename To>
    struct is_convertible_to
        : integral_constant<bool, is_convertible_to_impl<From, To>::value>
    {
    };

    // Helper variable template (C++14)
    template <typename From, typename To>
    inline constexpr bool is_convertible_to_v = is_convertible_to<From, To>::value;

    template <typename From, typename To>
    concept convertible_to = is_convertible_v<From, To> && requires { static_cast<To>(declval<From>()); };

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

    namespace detail
    {
        // Reference T: test real direct-initialization by passing the argument to a
        // declared (never defined/called - fine inside noexcept, same as everything
        // else here) function whose parameter type is exactly T. Passing an argument
        // to a function parameter is direct-initialization only; unlike "T(args...)"
        // it has no reinterpret_cast fallback, so it can't quietly accept things that
        // aren't real reference bindings.
        template <class T>
        void accept_as(T) noexcept;

        template <class T, class... Args>
        auto test(int)
            -> enable_if_t<is_reference_v<T>,
                           bool_constant<noexcept(accept_as<T>(declval<Args>()...))>>;

        // Non-reference T: functional-cast notation is fine here - for a non-class type
        // it's just a conversion, and for a class type it's overload resolution against
        // its constructors, same as direct-init would do anyway.
        template <class T, class... Args>
        auto test(int)
            -> enable_if_t<!is_reference_v<T>,
                           bool_constant<noexcept(T(declval<Args>()...))>>;

        template <class, class...>
        auto test(...) -> false_type;
    }

    template <class T, class... Args>
    struct is_nothrow_constructible
        : decltype(detail::test<T, Args...>(0))
    {
    };

    template <class T, class... Args>
    inline constexpr bool is_nothrow_constructible_v =
        is_nothrow_constructible<T, Args...>::value;

    template <class Type>
    constexpr bool is_object_v = is_const_v<const Type> && !is_void_v<Type>;

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

    namespace detail
    {
        template <typename T>
        struct is_destructible_scalar
        {
        private:
            template <typename U, typename = decltype(declval<U &>().~U())>
            static true_type test(int);
            template <typename>
            static false_type test(...);

        public:
            using type = decltype(test<T>(0));
        };

        template <typename T>
        struct is_nothrow_destructible_scalar
        {
        private:
            template <typename U>
            static bool_constant<noexcept(declval<U &>().~U())> test(int);
            template <typename>
            static false_type test(...);

        public:
            using type = decltype(test<T>(0));
        };
    } // namespace detail

    template <typename T>
    struct is_destructible;
    template <typename T>
    struct is_nothrow_destructible;

    namespace detail
    {
        enum class _destruct_cat
        {
            _ref,
            _invalid,
            _arr,
            _scalar
        };

        template <typename T>
        struct _is_bounded_array : false_type
        {
        };
        template <typename T, size_t N>
        struct _is_bounded_array<T[N]> : true_type
        {
        };

        template <typename T>
        struct _is_unbounded_array : false_type
        {
        };
        template <typename T>
        struct _is_unbounded_array<T[]> : true_type
        {
        };

        template <typename T>
        constexpr _destruct_cat _classify_destruct()
        {
            if constexpr (is_reference_v<T>)
                return _destruct_cat::_ref;
            else if constexpr (is_void_v<T> || is_function_v<T> || _is_unbounded_array<T>::value)
                return _destruct_cat::_invalid;
            else if constexpr (_is_bounded_array<T>::value)
                return _destruct_cat::_arr;
            else
                return _destruct_cat::_scalar;
        }

        template <typename T, _destruct_cat = _classify_destruct<T>()>
        struct _is_destructible_cat;

        template <typename T>
        struct _is_destructible_cat<T, _destruct_cat::_ref> : true_type
        {
        };
        template <typename T>
        struct _is_destructible_cat<T, _destruct_cat::_invalid> : false_type
        {
        };
        template <typename T>
        struct _is_destructible_cat<T, _destruct_cat::_arr>
            : integral_constant<bool, is_destructible<remove_extent_t<T>>::value>
        {
        };
        template <typename T>
        struct _is_destructible_cat<T, _destruct_cat::_scalar>
            : is_destructible_scalar<remove_cv_t<T>>::type
        {
        };

        template <typename T, _destruct_cat = _classify_destruct<T>()>
        struct _is_nothrow_destructible_cat;

        template <typename T>
        struct _is_nothrow_destructible_cat<T, _destruct_cat::_ref> : true_type
        {
        };
        template <typename T>
        struct _is_nothrow_destructible_cat<T, _destruct_cat::_invalid> : false_type
        {
        };
        template <typename T>
        struct _is_nothrow_destructible_cat<T, _destruct_cat::_arr>
            : integral_constant<bool, is_nothrow_destructible<remove_extent_t<T>>::value>
        {
        };
        template <typename T>
        struct _is_nothrow_destructible_cat<T, _destruct_cat::_scalar>
            : is_nothrow_destructible_scalar<remove_cv_t<T>>::type
        {
        };
    } // namespace detail

    template <class Type>
    using _Remove_cvref_t = remove_cv_t<remove_reference_t<Type>>;

    template <typename T>
    struct is_destructible : detail::_is_destructible_cat<T>
    {
    };
    template <typename T>
    inline constexpr bool is_destructible_v = is_destructible<T>::value;

    template <typename T>
    struct is_nothrow_destructible : detail::_is_nothrow_destructible_cat<T>
    {
    };
    template <typename T>
    inline constexpr bool is_nothrow_destructible_v = is_nothrow_destructible<T>::value;

    namespace detail
    {
        template <typename T>
        struct decay_impl
        {
        private:
            using U = remove_reference_t<T>;

        public:
            using type = conditional_t<is_array_v<U>,
                                       remove_extent_t<U> *,
                                       remove_cv_t<U>>;
        };
    } // namespace detail

    template <typename T>
    struct decay : detail::decay_impl<T>
    {
    };
    template <typename T>
    using decay_t = typename decay<T>::type;

    template <class...>
    struct common_reference;

    // Zero types
    template <>
    struct common_reference<>
    {
    };

    // One type
    template <class T>
    struct common_reference<T>
    {
        using type = T;
    };

    namespace detail
    {
        // Applies the const/volatile qualification of `From` onto `To`.
        template <typename From, typename To>
        struct copy_cv
        {
            using type = To;
        };
        template <typename From, typename To>
        struct copy_cv<const From, To>
        {
            using type = const To;
        };
        template <typename From, typename To>
        struct copy_cv<volatile From, To>
        {
            using type = volatile To;
        };
        template <typename From, typename To>
        struct copy_cv<const volatile From, To>
        {
            using type = const volatile To;
        };
        template <typename From, typename To>
        using copy_cv_t = typename copy_cv<From, To>::type;

        // COND-RES(X, Y)
        template <typename T1, typename T2>
        using cond_res_t = decltype(false ? declval<T1 (&)()>()() : declval<T2 (&)()>()());

        // COND-RES(COPYCV(X, Y)&, COPYCV(Y, X)&)
        template <typename X, typename Y>
        using condres_cvref_t = cond_res_t<copy_cv_t<X, Y> &, copy_cv_t<Y, X> &>;

        template <typename A, typename B, typename = void>
        struct common_ref_impl
        {
        };

        // Both A and B are lvalue references.
        template <typename X, typename Y>
        struct common_ref_impl<X &, Y &, void_t<condres_cvref_t<X, Y>>>
            : enable_if<is_reference_v<condres_cvref_t<X, Y>>, condres_cvref_t<X, Y>>
        {
        };

        template <typename A, typename B>
        using common_ref_t_ = typename common_ref_impl<A, B>::type;

        // C = remove_reference_t<COMMON-REF(X&, Y&)>&&
        template <typename X, typename Y>
        using common_ref_C = remove_reference_t<common_ref_t_<X &, Y &>> &&;

        // Both A and B are rvalue references.
        template <typename X, typename Y>
        struct common_ref_impl<X &&, Y &&,
                               enable_if_t<is_convertible_v<X &&, common_ref_C<X, Y>> &&
                                           is_convertible_v<Y &&, common_ref_C<X, Y>>>>
        {
            using type = common_ref_C<X, Y>;
        };

        // D = COMMON-REF(const X&, Y&)
        template <typename X, typename Y>
        using common_ref_D = common_ref_t_<const X &, Y &>;

        // A is an rvalue reference, B is an lvalue reference.
        template <typename X, typename Y>
        struct common_ref_impl<X &&, Y &, enable_if_t<is_convertible_v<X &&, common_ref_D<X, Y>>>>
        {
            using type = common_ref_D<X, Y>;
        };

        // A is an lvalue reference, B is an rvalue reference: delegate, swapped.
        template <typename X, typename Y>
        struct common_ref_impl<X &, Y &&> : common_ref_impl<Y &&, X &>
        {
        };

        // ---- dispatch: try COMMON-REF for reference pairs, else COND-RES ----

        template <typename T1, typename T2, typename = void>
        struct common_reference_condres
        {
        };
        template <typename T1, typename T2>
        struct common_reference_condres<T1, T2, void_t<cond_res_t<T1, T2>>>
        {
            using type = cond_res_t<T1, T2>;
        };

        template <typename T1, typename T2, typename = void>
        struct common_reference_refs
        {
        };
        template <typename T1, typename T2>
        struct common_reference_refs<
            T1, T2,
            void_t<enable_if_t<is_reference_v<T1> && is_reference_v<T2>>, common_ref_t_<T1, T2>>>
        {
            using type = common_ref_t_<T1, T2>;
        };

        template <typename T1, typename T2, typename = void>
        struct common_reference_2 : common_reference_condres<T1, T2>
        {
        };
        template <typename T1, typename T2>
        struct common_reference_2<T1, T2, void_t<typename common_reference_refs<T1, T2>::type>>
            : common_reference_refs<T1, T2>
        {
        };
    } // namespace detail

    // Two types
    template <class T1, class T2>
    struct common_reference<T1, T2> : detail::common_reference_2<T1, T2>
    {
    };

    // Three or more types: fold pairwise, left to right
    template <class T1, class T2, class... Rest>
    struct common_reference<T1, T2, Rest...>
        : common_reference<typename common_reference<T1, T2>::type, Rest...>
    {
    };

    template <class... Ts>
    using common_reference_t = typename common_reference<Ts...>::type;

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

    template <typename T>
    constexpr enable_if_t<is_move_constructible_v<T> && is_move_assignable_v<T>>
    swap(T &a, T &b) noexcept(is_nothrow_move_constructible_v<T> &&
                              is_nothrow_move_assignable_v<T>)
    {
        T tmp = move(a);
        a = move(b);
        b = move(tmp);
    }

    template <class Type>
    constexpr conditional_t<!is_nothrow_move_constructible_v<Type> && is_copy_constructible_v<Type>, const Type &, Type &&>
    move_if_noexcept(Type &_Arg) noexcept
    {
        return SFTL::move(_Arg);
    }

    using std::is_constant_evaluated;

    using std::is_trivial;
    using std::is_trivial_v;

    using std::is_trivially_assignable;
    using std::is_trivially_assignable_v;

    using std::is_trivially_move_constructible;
    using std::is_trivially_move_constructible_v;

    using std::is_trivially_move_assignable;
    using std::is_trivially_move_assignable_v;

    using std::is_trivially_constructible;
    using std::is_trivially_constructible_v;

    using std::is_trivially_copy_assignable;
    using std::is_trivially_copy_assignable_v;

    using std::is_trivially_copy_constructible;
    using std::is_trivially_copy_constructible_v;

    using std::is_trivially_copyable;
    using std::is_trivially_copyable_v;

    using std::is_trivially_default_constructible;
    using std::is_trivially_default_constructible_v;

    using std::is_trivially_destructible;
    using std::is_trivially_destructible_v;

} // namespace SFTL