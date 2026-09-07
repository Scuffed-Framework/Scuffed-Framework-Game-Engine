#pragma once
#include "TypeTraits.hpp"
#include "Types.hpp"

namespace SFTL
{
    // Helper to check if a pointer type defines its own nested rebind template
    template<typename Ptr, typename U, typename = void>
    struct has_pointer_traits_rebind : false_type
    {
    };

    template<typename Ptr, typename U>
    struct has_pointer_traits_rebind<Ptr, U, void_t<typename Ptr::template rebind<U>>> : true_type
    {
    };

    // Primary template for pointer_traits
    template<typename Ptr>
    struct pointer_traits
    {
        using pointer         = Ptr;
        using element_type    = typename Ptr::element_type;
        using difference_type = typename Ptr::difference_type;

        // Rebind helper template
        template<typename U>
        using rebind = conditional_t<has_pointer_traits_rebind<Ptr, U>::value, typename Ptr::template rebind<U>,
                                     /* Fallback or generic rebind logic if needed */
                                     void>;

        [[nodiscard]] static constexpr pointer pointer_to(element_type &r) noexcept { return Ptr::pointer_to(r); }
    };

    // Specialization for raw pointer types (T*)
    template<typename T>
    struct pointer_traits<T *>
    {
        using pointer         = T *;
        using element_type    = T;
        using difference_type = ptrdiff_t;

        template<typename U>
        using rebind = U *;

        [[nodiscard]] static constexpr pointer pointer_to(element_type &r) noexcept { return ::SFTL::addressof(r); }
    };

    // Specialization for const raw pointer types (const T*)
    template<typename T>
    struct pointer_traits<const T *>
    {
        using pointer         = const T *;
        using element_type    = const T;
        using difference_type = ptrdiff_t;

        template<typename U>
        using rebind = U *;

        [[nodiscard]] static constexpr pointer pointer_to(element_type &r) noexcept { return ::SFTL::addressof(r); }
    };

    template<typename Ptr, typename U>
    using ptr_rebind = typename pointer_traits<Ptr>::template rebind<U>;

    // Utility function: to_address support
    template<typename Ptr>
    [[nodiscard]] constexpr auto to_address(const Ptr &p) noexcept
    {
        return pointer_traits<Ptr>::to_address(p);
    }

    template<typename T>
    [[nodiscard]] constexpr T *to_address(T *p) noexcept
    {
        static_assert(!is_function_v<T>, "Cannot convert a function pointer to an address using to_address");
        return p;
    }
} // namespace SFTL
