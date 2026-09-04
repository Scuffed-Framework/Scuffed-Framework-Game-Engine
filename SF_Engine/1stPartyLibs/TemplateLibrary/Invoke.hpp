#pragma once

#include "TypeTraits.hpp"

namespace SFTL
{
    namespace Detail
    {
        template<typename T>
        struct MemberPointerClass;

        template<typename T, typename C>
        struct MemberPointerClass<T C::*>
        {
            using type = C;
        };

        template<typename T>
        using member_pointer_class_t = typename MemberPointerClass<remove_cv_t<T>>::type;


        template<typename Fn, typename T, typename... Args>
        constexpr decltype(auto) InvokeMemberFunction(Fn &&fn, T &&object, Args &&...args)
        {
            using Class = member_pointer_class_t<remove_cvref_t<Fn>>;

            if constexpr (is_base_of_v<Class, remove_cvref_t<T>>)
            {
                return (forward<T>(object).*forward<Fn>(fn))(forward<Args>(args)...);
            } else
            {
                return ((*forward<T>(object)).*forward<Fn>(fn))(forward<Args>(args)...);
            }
        }


        template<typename Fn, typename T>
        constexpr decltype(auto) InvokeMemberObject(Fn &&fn, T &&object)
        {
            using Class = member_pointer_class_t<remove_cvref_t<Fn>>;

            if constexpr (is_base_of_v<Class, remove_cvref_t<T>>)
            {
                return forward<T>(object).*forward<Fn>(fn);
            } else
            {
                return (*forward<T>(object)).*forward<Fn>(fn);
            }
        }
    } // namespace Detail


    template<typename Fn, typename T, typename... Args>
        requires is_member_function_pointer_v<remove_cvref_t<Fn>>
    constexpr decltype(auto) invoke(Fn &&fn, T &&object, Args &&...args)
    {
        return Detail::InvokeMemberFunction(forward<Fn>(fn), forward<T>(object), forward<Args>(args)...);
    }


    template<typename Fn, typename T>
        requires is_member_object_pointer_v<remove_cvref_t<Fn>>
    constexpr decltype(auto) invoke(Fn &&fn, T &&object)
    {
        return Detail::InvokeMemberObject(forward<Fn>(fn), forward<T>(object));
    }


    template<typename Fn, typename... Args>
        requires(!is_member_pointer_v<remove_cvref_t<Fn>>)
    constexpr decltype(auto) invoke(Fn &&fn, Args &&...args)
    {
        return forward<Fn>(fn)(forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    struct invoke_result
    {
        using type = decltype(SFTL::invoke(SFTL::declval<Fn>(), SFTL::declval<Args>()...));
    };

    template<typename Fn, typename... Args>
    using invoke_result_t = typename invoke_result<Fn, Args...>::type;
} // namespace SFTL
