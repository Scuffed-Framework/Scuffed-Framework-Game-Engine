/******************************************************************************/
/* Allocator.hpp                                                             */
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
#include <new>
#include "TypeTraits.hpp"
#include "Types.hpp"

namespace SFTL
{
    template<typename T>
    class allocator
    {
        static_assert(!is_const_v<T>, "SFTL::allocator<const T> is ill-formed");
        static_assert(!is_function_v<T>, "SFTL::allocator does not support function types");

    public:
        using value_type      = T;
        using pointer         = T *;
        using const_pointer   = const T *;
        using reference       = T &;
        using const_reference = const T &;
        using difference_type = ::SFTL::ptrdiff_t;

        using propagate_on_container_move_assignment = true_type;
        using propagate_on_container_copy_assignment = false_type;
        using propagate_on_container_swap            = false_type;
        using is_always_equal                        = true_type;

        template<typename U>
        struct rebind
        {
            using other = allocator<U>;
        };

        constexpr allocator() noexcept                  = default;
        constexpr allocator(const allocator &) noexcept = default;

        template<typename U>
        constexpr allocator(const allocator<U> &) noexcept
        {
        }

        allocator &operator=(const allocator &) = default;
        ~allocator()                            = default;

        // Returns nullptr on failure  never throws, regardless of n.
        [[nodiscard]] T *allocate(size_type n) noexcept
        {
            if (n > max_size())
                return nullptr;

            if (is_constant_evaluated())
                return static_cast<T *>(::operator new(n * sizeof(T)));

            return static_cast<T *>(::operator new(n * sizeof(T), ::std::align_val_t(alignof(T)), ::std::nothrow));
        }

        constexpr void deallocate(T *p, size_type) noexcept
        {
            if (!p)
                return;
            ::operator delete(p, ::std::align_val_t(alignof(T)));
        }

        template<typename U, typename... Args>
        constexpr void construct(U *p, Args &&...args) noexcept(is_nothrow_constructible_v<U, Args...>)
        {
            ::new (static_cast<void *>(p)) U(::SFTL::forward<Args>(args)...);
        }

        template<typename U>
        constexpr void destroy(U *p) noexcept
        {
            p->~U();
        }

        constexpr size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(T); }

        template<typename U>
        friend class allocator;
    };

    template<typename T, typename U>
    constexpr bool operator==(const allocator<T> &, const allocator<U> &) noexcept
    {
        return true;
    }

    template<typename T, typename U>
    constexpr bool operator!=(const allocator<T> &, const allocator<U> &) noexcept
    {
        return false;
    }

    namespace Detail
    {
        template<typename T>
        T &&DeclVal() noexcept;

        template<typename Alloc, typename = void>
        struct AllocPointer
        {
            using type = typename Alloc::value_type *;
        };
        template<typename Alloc>
        struct AllocPointer<Alloc, void_t<typename Alloc::pointer>>
        {
            using type = typename Alloc::pointer;
        };

        template<typename Alloc, typename Pointer, typename = void>
        struct AllocConstPointer
        {
            using type = const typename Alloc::value_type *;
        };
        template<typename Alloc, typename Pointer>
        struct AllocConstPointer<Alloc, Pointer, void_t<typename Alloc::const_pointer>>
        {
            using type = typename Alloc::const_pointer;
        };

        template<typename Alloc, typename = void>
        struct AllocVoidPointer
        {
            using type = void *;
        };
        template<typename Alloc>
        struct AllocVoidPointer<Alloc, void_t<typename Alloc::void_pointer>>
        {
            using type = typename Alloc::void_pointer;
        };

        template<typename Alloc, typename = void>
        struct AllocConstVoidPointer
        {
            using type = const void *;
        };
        template<typename Alloc>
        struct AllocConstVoidPointer<Alloc, void_t<typename Alloc::const_void_pointer>>
        {
            using type = typename Alloc::const_void_pointer;
        };

        template<typename Alloc, typename = void>
        struct AllocDifferenceType
        {
            using type = ::SFTL::ptrdiff_t;
        };
        template<typename Alloc>
        struct AllocDifferenceType<Alloc, void_t<typename Alloc::difference_type>>
        {
            using type = typename Alloc::difference_type;
        };

        template<typename Alloc, typename = void>
        struct AllocSizeType
        {
            using type = ::SFTL::size_type;
        };
        template<typename Alloc>
        struct AllocSizeType<Alloc, void_t<typename Alloc::size_type>>
        {
            using type = typename Alloc::size_type;
        };

        template<typename Alloc, typename = void>
        struct AllocPOCCA
        {
            using type = false_type;
        };
        template<typename Alloc>
        struct AllocPOCCA<Alloc, void_t<typename Alloc::propagate_on_container_copy_assignment>>
        {
            using type = typename Alloc::propagate_on_container_copy_assignment;
        };

        template<typename Alloc, typename = void>
        struct AllocPOCMA
        {
            using type = false_type;
        };
        template<typename Alloc>
        struct AllocPOCMA<Alloc, void_t<typename Alloc::propagate_on_container_move_assignment>>
        {
            using type = typename Alloc::propagate_on_container_move_assignment;
        };

        template<typename Alloc, typename = void>
        struct AllocPOCS
        {
            using type = false_type;
        };
        template<typename Alloc>
        struct AllocPOCS<Alloc, void_t<typename Alloc::propagate_on_container_swap>>
        {
            using type = typename Alloc::propagate_on_container_swap;
        };

        template<typename Alloc, typename = void>
        struct AllocIsAlwaysEqual
        {
            using type = std::conditional_t<std::is_empty_v<Alloc>, true_type, false_type>;
        };
        template<typename Alloc>
        struct AllocIsAlwaysEqual<Alloc, void_t<typename Alloc::is_always_equal>>
        {
            using type = typename Alloc::is_always_equal;
        };

        template<typename Alloc, typename Pointer, typename... Args>
        class HasConstruct
        {
            template<typename A2,
                     typename = decltype(DeclVal<A2 &>().construct(DeclVal<Pointer>(), DeclVal<Args>()...))>
            static true_type Test(int);
            template<typename>
            static false_type Test(...);

        public:
            static constexpr bool value = decltype(Test<Alloc>(0))::value;
        };

        template<typename Alloc, typename Pointer>
        class HasDestroy
        {
            template<typename A2, typename = decltype(DeclVal<A2 &>().destroy(DeclVal<Pointer>()))>
            static true_type Test(int);
            template<typename>
            static false_type Test(...);

        public:
            static constexpr bool value = decltype(Test<Alloc>(0))::value;
        };

        template<typename Alloc>
        class HasMaxSize
        {
            template<typename A2, typename = decltype(DeclVal<const A2 &>().max_size())>
            static true_type Test(int);
            template<typename>
            static false_type Test(...);

        public:
            static constexpr bool value = decltype(Test<Alloc>(0))::value;
        };

        template<typename Alloc>
        class HasSelectOnCopy
        {
            template<typename A2, typename = decltype(DeclVal<const A2 &>().select_on_container_copy_construction())>
            static true_type Test(int);
            template<typename>
            static false_type Test(...);

        public:
            static constexpr bool value = decltype(Test<Alloc>(0))::value;
        };

        template<typename Alloc, typename SizeType, typename ConstVoidPointer>
        class HasAllocateHint
        {
            template<typename A2,
                     typename = decltype(DeclVal<A2 &>().allocate(DeclVal<SizeType>(), DeclVal<ConstVoidPointer>()))>
            static true_type Test(int);
            template<typename>
            static false_type Test(...);

        public:
            static constexpr bool value = decltype(Test<Alloc>(0))::value;
        };
        template<typename Alloc, typename U, typename = void>
        struct AllocRebind
        {
        }; // intentionally empty: no ::type when Alloc has no rebind<U>::other
        template<typename Alloc, typename U>
        struct AllocRebind<Alloc, U, void_t<typename Alloc::template rebind<U>::other>>
        {
            using type = typename Alloc::template rebind<U>::other;
        };
    } // namespace Detail

    template<typename Alloc>
    struct allocator_traits
    {
        using allocator_type = Alloc;
        using value_type     = typename Alloc::value_type;

        using pointer            = typename Detail::AllocPointer<Alloc>::type;
        using const_pointer      = typename Detail::AllocConstPointer<Alloc, pointer>::type;
        using void_pointer       = typename Detail::AllocVoidPointer<Alloc>::type;
        using const_void_pointer = typename Detail::AllocConstVoidPointer<Alloc>::type;

        using difference_type = typename Detail::AllocDifferenceType<Alloc>::type;
        using alloc_size_type = typename Detail::AllocSizeType<Alloc>::type;

        using propagate_on_container_copy_assignment = typename Detail::AllocPOCCA<Alloc>::type;
        using propagate_on_container_move_assignment = typename Detail::AllocPOCMA<Alloc>::type;
        using propagate_on_container_swap            = typename Detail::AllocPOCS<Alloc>::type;
        using is_always_equal                        = typename Detail::AllocIsAlwaysEqual<Alloc>::type;

        template<typename U>
        using rebind_alloc = typename Detail::AllocRebind<Alloc, U>::type;

        template<typename U>
        using rebind_traits = allocator_traits<rebind_alloc<U>>;

        [[nodiscard]] static pointer allocate(Alloc &a, alloc_size_type n) { return a.allocate(n); }

        [[nodiscard]] static pointer allocate(Alloc &a, alloc_size_type n, const_void_pointer hint)
        {
            if constexpr (Detail::HasAllocateHint<Alloc, alloc_size_type, const_void_pointer>::value)
                return a.allocate(n, hint);
            else
                return a.allocate(n);
        }

        static void deallocate(Alloc &a, pointer p, alloc_size_type n) { a.deallocate(p, n); }

        template<typename T, typename... Args>
        static void construct(Alloc &a, T *p, Args &&...args)
        {
            if constexpr (Detail::HasConstruct<Alloc, T *, Args...>::value)
                a.construct(p, ::SFTL::forward<Args>(args)...);
            else
                ::new (static_cast<void *>(p)) T(::SFTL::forward<Args>(args)...);
        }

        template<typename T>
        static void destroy(Alloc &a, T *p)
        {
            if constexpr (Detail::HasDestroy<Alloc, T *>::value)
                a.destroy(p);
            else
                p->~T();
        }

        static alloc_size_type max_size(const Alloc &a) noexcept
        {
            if constexpr (Detail::HasMaxSize<Alloc>::value)
                return a.max_size();
            else
                return static_cast<alloc_size_type>(-1) / sizeof(value_type);
        }

        static Alloc select_on_container_copy_construction(const Alloc &a)
        {
            if constexpr (Detail::HasSelectOnCopy<Alloc>::value)
                return a.select_on_container_copy_construction();
            else
                return a;
        }
    };
} // namespace SFTL
