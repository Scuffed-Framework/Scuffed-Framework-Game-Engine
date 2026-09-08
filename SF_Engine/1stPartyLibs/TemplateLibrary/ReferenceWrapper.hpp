#pragma once
#include "TypeTraits.hpp"

namespace SFTL
{
    template<typename T>
    class reference_wrapper
    {
    public:
        using type = T;

    private:
        T *_M_ptr;

    public:
        template<typename _Up>
        constexpr reference_wrapper(_Up &&) = delete;

        template<typename _Up, typename = enable_if_t<!is_same_v<reference_wrapper, remove_cvref_t<_Up>> &&
                                                      is_convertible_v<remove_reference_t<_Up> *, T *>>>
        constexpr reference_wrapper(_Up &uref) noexcept : _M_ptr(addressof(uref))
        {
        }

        // Copy constructor
        constexpr reference_wrapper(const reference_wrapper &_In) noexcept = default;

        // Assignment operator
        constexpr reference_wrapper &operator=(const reference_wrapper &_In) noexcept = default;

        // Accessors & Conversion
        constexpr operator T &() const noexcept { return *_M_ptr; }

        constexpr T &get() const noexcept { return *_M_ptr; }

        template<typename... _Args>
        constexpr auto operator()(_Args &&...args) const noexcept(noexcept(invoke(get(), forward<_Args>(args)...)))
                -> decltype(invoke(get(), forward<_Args>(args)...))
        {
            return invoke(get(), forward<_Args>(args)...);
        }
    };

    template<typename T>
    [[nodiscard]] constexpr reference_wrapper<T> ref(T &t) noexcept
    {
        return reference_wrapper<T>(t);
    }

    template<typename T>
    void ref(const T &&) = delete;

    template<typename T>
    [[nodiscard]] constexpr reference_wrapper<const T> cref(const T &t) noexcept
    {
        return reference_wrapper<const T>(t);
    }

    template<typename T>
    void cref(const T &&) = delete;

    template<typename _Tp>
    struct unwrap_reference
    {
        using type = _Tp;
    };

    template<typename _Tp>
    struct unwrap_reference<reference_wrapper<_Tp>>
    {
        using type = _Tp &;
    };

    template<typename _Tp>
    using unwrap_reference_t = unwrap_reference<_Tp>::type;

    template<typename _Tp>
    struct unwrap_ref_decay
    {
        using type = unwrap_reference_t<decay_t<_Tp>>;
    };

    template<typename _Tp>
    using unwrap_ref_decay_t = unwrap_ref_decay<_Tp>::type;

} // namespace SFTL
