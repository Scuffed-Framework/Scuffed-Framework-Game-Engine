#pragma once
#include <1stPartyLibs/TemplateLibrary/Memory.hpp>
#include <1stPartyLibs/TemplateLibrary/TypeTraits.hpp>
#include "RTTI.hpp"

namespace SF::RTTI
{
    template<typename T, typename U>
    T *rtti_cast(U *ptr)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        static_assert(HasRtti<U>, "Source type has no SF_RTTI / SF_RTTI_BASE declared.");
        if (ptr && ptr->RTTI_IsTypeOf(T::RTTI_Type()))
        {
            return static_cast<T *>(ptr);
        }
        return nullptr;
    }

    template<typename T, typename U>
    const T *rtti_cast(const U *ptr)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        static_assert(HasRtti<U>, "Source type has no SF_RTTI / SF_RTTI_BASE declared.");
        if (ptr && ptr->RTTI_IsTypeOf(T::RTTI_Type()))
        {
            return static_cast<const T *>(ptr);
        }
        return nullptr;
    }

    template<typename T, typename U>
    bool rtti_istypeof(const U *ptr)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        return ptr && ptr->RTTI_IsTypeOf(T::RTTI_Type());
    }

    template<typename T, typename U, typename = std::enable_if_t<!std::is_pointer_v<U>>>
    bool rtti_istypeof(const U &ref)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        return ref.RTTI_IsTypeOf(T::RTTI_Type());
    }

    template<typename T, typename U>
    bool rtti_isexactly(const U &ref)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        return ref.RTTI_GetType() == T::RTTI_Type();
    }

    template<typename T, typename U>
    std::shared_ptr<T> rtti_pointer_cast(const std::shared_ptr<U> &ptr)
    {
        static_assert(HasRtti<T>, "Target type has no SF_RTTI / SF_RTTI_BASE declared.");
        static_assert(HasRtti<U>, "Source type has no SF_RTTI / SF_RTTI_BASE declared.");
        if (ptr && ptr->RTTI_IsTypeOf(T::RTTI_Type()))
        {
            return std::shared_ptr<T>(ptr, static_cast<T *>(ptr.get()));
        }
        return nullptr;
    }
}