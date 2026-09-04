/******************************************************************************/
/* Atomic.hpp                                                                 */
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
#include "Types.hpp"
#include "TypeTraits.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#define SFTL_ATOMIC_MSVC 1
#include <intrin.h>
#else
#define SFTL_ATOMIC_GNU 1
#endif

namespace SFTL
{
    enum class memory_order
    {
        relaxed,
        consume,
        acquire,
        release,
        acq_rel,
        seq_cst
    };

    inline constexpr memory_order memory_order_relaxed = memory_order::relaxed;
    inline constexpr memory_order memory_order_consume = memory_order::consume;
    inline constexpr memory_order memory_order_acquire = memory_order::acquire;
    inline constexpr memory_order memory_order_release = memory_order::release;
    inline constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
    inline constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

#if defined(SFTL_ATOMIC_GNU)
    namespace detail
    {
        constexpr int _to_gnu_order(memory_order order) noexcept
        {
            switch (order)
            {
            case memory_order::relaxed:
                return __ATOMIC_RELAXED;
            case memory_order::consume:
                return __ATOMIC_CONSUME;
            case memory_order::acquire:
                return __ATOMIC_ACQUIRE;
            case memory_order::release:
                return __ATOMIC_RELEASE;
            case memory_order::acq_rel:
                return __ATOMIC_ACQ_REL;
            case memory_order::seq_cst:
            default:
                return __ATOMIC_SEQ_CST;
            }
        }
    } // namespace detail
#endif

    template <typename T>
    class atomic
    {
        static_assert(is_integral_v<T> || is_pointer_v<T>,
                      "SFTL::atomic currently only supports integral and pointer types");

    public:
        using value_type = T;

        atomic() noexcept = default;
        constexpr atomic(T desired) noexcept : _value(desired) {}

        atomic(const atomic &) = delete;
        atomic &operator=(const atomic &) = delete;
        atomic &operator=(const atomic &) volatile = delete;

        T load(memory_order order = memory_order::seq_cst) const noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            return __atomic_load_n(&_value, detail::_to_gnu_order(order));
#else
            (void)order;
            ::_ReadWriteBarrier();
            T result = _value;
            ::_ReadWriteBarrier();
            return result;
#endif
        }

        void store(T desired, memory_order order = memory_order::seq_cst) noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            __atomic_store_n(&_value, desired, detail::_to_gnu_order(order));
#else
            (void)order;
            ::_ReadWriteBarrier();
            _value = desired;
            ::_ReadWriteBarrier();
#endif
        }

        T exchange(T desired, memory_order order = memory_order::seq_cst) noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            return __atomic_exchange_n(&_value, desired, detail::_to_gnu_order(order));
#else
            (void)order;
            return static_cast<T>(_msvc_exchange(desired));
#endif
        }

        T fetch_add(T arg, memory_order order = memory_order::seq_cst) noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            return __atomic_fetch_add(&_value, arg, detail::_to_gnu_order(order));
#else
            (void)order;
            return static_cast<T>(_msvc_fetch_add(arg));
#endif
        }

        T fetch_sub(T arg, memory_order order = memory_order::seq_cst) noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            return __atomic_fetch_sub(&_value, arg, detail::_to_gnu_order(order));
#else
            (void)order;
            return static_cast<T>(_msvc_fetch_add(static_cast<T>(0 - arg)));
#endif
        }

        bool compare_exchange_weak(T &expected, T desired,
                                   memory_order success = memory_order::seq_cst,
                                   memory_order failure = memory_order::seq_cst) noexcept
        {
            return compare_exchange_strong(expected, desired, success, failure);
        }

        bool compare_exchange_strong(T &expected, T desired,
                                     memory_order success = memory_order::seq_cst,
                                     memory_order failure = memory_order::seq_cst) noexcept
        {
#if defined(SFTL_ATOMIC_GNU)
            return __atomic_compare_exchange_n(
                &_value, &expected, desired,
                /*weak=*/false,
                detail::_to_gnu_order(success),
                detail::_to_gnu_order(failure));
#else
            (void)success;
            (void)failure;
            T prev = static_cast<T>(_msvc_cas(expected, desired));
            if (prev == expected)
                return true;
            expected = prev;
            return false;
#endif
        }

        T operator++() noexcept { return fetch_add(T(1)) + T(1); }
        T operator++(int) noexcept { return fetch_add(T(1)); }
        T operator--() noexcept { return fetch_sub(T(1)) - T(1); }
        T operator--(int) noexcept { return fetch_sub(T(1)); }

        operator T() const noexcept { return load(); }
        T operator=(T desired) noexcept
        {
            store(desired);
            return desired;
        }

    private:
#if defined(SFTL_ATOMIC_MSVC)
        long long _msvc_exchange(T desired) noexcept
        {
            if constexpr (sizeof(T) == 4)
                return ::_InterlockedExchange(reinterpret_cast<long *>(&_value),
                                              static_cast<long>(desired));
            else
                return ::_InterlockedExchange64(reinterpret_cast<long long *>(&_value),
                                                static_cast<long long>(desired));
        }

        long long _msvc_fetch_add(T arg) noexcept
        {
            if constexpr (sizeof(T) == 4)
                return ::_InterlockedExchangeAdd(reinterpret_cast<long *>(&_value),
                                                 static_cast<long>(arg));
            else
                return ::_InterlockedExchangeAdd64(reinterpret_cast<long long *>(&_value),
                                                   static_cast<long long>(arg));
        }

        long long _msvc_cas(T expected, T desired) noexcept
        {
            if constexpr (sizeof(T) == 4)
                return ::_InterlockedCompareExchange(reinterpret_cast<long *>(&_value),
                                                     static_cast<long>(desired),
                                                     static_cast<long>(expected));
            else
                return ::_InterlockedCompareExchange64(reinterpret_cast<long long *>(&_value),
                                                       static_cast<long long>(desired),
                                                       static_cast<long long>(expected));
        }
#endif

        T _value{};
    };
} // namespace SFTL