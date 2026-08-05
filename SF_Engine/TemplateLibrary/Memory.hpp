/******************************************************************************/
/* Memory.hpp                                                                 */
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
#include "Atomic.hpp"
#include <new>

namespace SFTL
{
    // just the worst implementation of <memory> ever

    template <class Type>
    struct default_delete
    {
        constexpr default_delete() noexcept = default;

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        constexpr default_delete(const default_delete<_Uty> &) noexcept {}

        constexpr void operator()(Type *_Ptr) const noexcept
        {
            static_assert(0 < sizeof(Type), "can't delete an incomplete type");
            delete _Ptr;
        }
    };

    template <class Type>
    struct default_delete<Type[]>
    {
        constexpr default_delete() noexcept = default;

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        constexpr default_delete(const default_delete<_Uty[]> &) noexcept {}

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        constexpr void operator()(_Uty *_Ptr) const noexcept
        {
            static_assert(0 < sizeof(_Uty), "can't delete an incomplete type");
            delete[] _Ptr;
        }
    };

    namespace detail
    {
        template <class _Dx, class Type, typename = void>
        struct _unique_ptr_pointer
        {
            using type = Type *;
        };
        template <class _Dx, class Type>
        struct _unique_ptr_pointer<_Dx, Type, void_t<typename remove_reference_t<_Dx>::pointer>>
        {
            using type = typename remove_reference_t<_Dx>::pointer;
        };
    } // namespace detail

    template <class Type, class _Dx = default_delete<Type>>
    class unique_ptr
    {
        static_assert(!is_reference_v<_Dx>,
                      "SFTL::unique_ptr does not support reference deleters");

    public:
        using element_type = Type;
        using deleter_type = _Dx;
        using pointer = typename detail::_unique_ptr_pointer<_Dx, Type>::type;

        constexpr unique_ptr() noexcept : _ptr(nullptr), _deleter() {}
        constexpr unique_ptr(decltype(nullptr)) noexcept : _ptr(nullptr), _deleter() {}

        explicit unique_ptr(pointer p) noexcept : _ptr(p), _deleter() {}
        unique_ptr(pointer p, const _Dx &d) noexcept : _ptr(p), _deleter(d) {}
        unique_ptr(pointer p, _Dx &&d) noexcept : _ptr(p), _deleter(::SFTL::move(d)) {}

        unique_ptr(const unique_ptr &) = delete;
        unique_ptr &operator=(const unique_ptr &) = delete;

        unique_ptr(unique_ptr &&other) noexcept
            : _ptr(other._ptr), _deleter(::SFTL::move(other._deleter))
        {
            other._ptr = nullptr;
        }

        template <class _Uty, class _Ex,
                  enable_if_t<!is_array_v<_Uty> &&
                                  is_convertible_v<typename unique_ptr<_Uty, _Ex>::pointer, pointer> &&
                                  is_convertible_v<_Ex, _Dx>,
                              int> = 0>
        unique_ptr(unique_ptr<_Uty, _Ex> &&other) noexcept
            : _ptr(other.release()), _deleter(::SFTL::forward<_Ex>(other.get_deleter()))
        {
        }

        unique_ptr &operator=(unique_ptr &&other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
                _deleter = ::SFTL::move(other._deleter);
            }
            return *this;
        }

        unique_ptr &operator=(decltype(nullptr)) noexcept
        {
            reset();
            return *this;
        }

        ~unique_ptr()
        {
            if (_ptr)
                _deleter(_ptr);
        }

        pointer release() noexcept
        {
            pointer old = _ptr;
            _ptr = nullptr;
            return old;
        }

        void reset(pointer p = pointer()) noexcept
        {
            pointer old = _ptr;
            _ptr = p;
            if (old)
                _deleter(old);
        }

        void swap(unique_ptr &other) noexcept
        {
            pointer tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;

            _Dx tmp_d = ::SFTL::move(_deleter);
            _deleter = ::SFTL::move(other._deleter);
            other._deleter = ::SFTL::move(tmp_d);
        }

        pointer get() const noexcept { return _ptr; }
        _Dx &get_deleter() noexcept { return _deleter; }
        const _Dx &get_deleter() const noexcept { return _deleter; }

        explicit operator bool() const noexcept { return _ptr != nullptr; }

        add_lvalue_reference_t<Type> operator*() const { return *_ptr; }
        pointer operator->() const noexcept { return _ptr; }

    private:
        pointer _ptr;
        _Dx _deleter;
    };

    template <class Type, class _Dx>
    class unique_ptr<Type[], _Dx>
    {
    public:
        using element_type = Type;
        using deleter_type = _Dx;
        using pointer = typename detail::_unique_ptr_pointer<_Dx, Type>::type;

        constexpr unique_ptr() noexcept : _ptr(nullptr), _deleter() {}
        constexpr unique_ptr(decltype(nullptr)) noexcept : _ptr(nullptr), _deleter() {}

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        explicit unique_ptr(_Uty p) noexcept : _ptr(p), _deleter() {}

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        unique_ptr(_Uty p, const _Dx &d) noexcept : _ptr(p), _deleter(d) {}

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        unique_ptr(_Uty p, _Dx &&d) noexcept : _ptr(p), _deleter(::SFTL::move(d)) {}

        unique_ptr(const unique_ptr &) = delete;
        unique_ptr &operator=(const unique_ptr &) = delete;

        unique_ptr(unique_ptr &&other) noexcept
            : _ptr(other._ptr), _deleter(::SFTL::move(other._deleter))
        {
            other._ptr = nullptr;
        }

        unique_ptr &operator=(unique_ptr &&other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
                _deleter = ::SFTL::move(other._deleter);
            }
            return *this;
        }

        unique_ptr &operator=(decltype(nullptr)) noexcept
        {
            reset();
            return *this;
        }

        ~unique_ptr()
        {
            if (_ptr)
                _deleter(_ptr);
        }

        pointer release() noexcept
        {
            pointer old = _ptr;
            _ptr = nullptr;
            return old;
        }

        template <class _Uty, enable_if_t<is_convertible_v<_Uty (*)[], Type (*)[]>, int> = 0>
        void reset(_Uty p) noexcept
        {
            pointer old = _ptr;
            _ptr = p;
            if (old)
                _deleter(old);
        }

        void reset(decltype(nullptr) = nullptr) noexcept
        {
            pointer old = _ptr;
            _ptr = nullptr;
            if (old)
                _deleter(old);
        }

        void swap(unique_ptr &other) noexcept
        {
            pointer tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;
            _Dx tmp_d = ::SFTL::move(_deleter);
            _deleter = ::SFTL::move(other._deleter);
            other._deleter = ::SFTL::move(tmp_d);
        }

        pointer get() const noexcept { return _ptr; }
        _Dx &get_deleter() noexcept { return _deleter; }
        const _Dx &get_deleter() const noexcept { return _deleter; }

        explicit operator bool() const noexcept { return _ptr != nullptr; }
        Type &operator[](size_t i) const { return _ptr[i]; }

    private:
        pointer _ptr;
        _Dx _deleter;
    };

    template <class T1, class D1, class T2, class D2>
    bool operator==(const unique_ptr<T1, D1> &a, const unique_ptr<T2, D2> &b) { return a.get() == b.get(); }
    template <class T1, class D1, class T2, class D2>
    bool operator!=(const unique_ptr<T1, D1> &a, const unique_ptr<T2, D2> &b) { return a.get() != b.get(); }
    template <class T, class D>
    bool operator==(const unique_ptr<T, D> &a, decltype(nullptr)) noexcept { return !a; }
    template <class T, class D>
    bool operator==(decltype(nullptr), const unique_ptr<T, D> &a) noexcept { return !a; }
    template <class T, class D>
    bool operator!=(const unique_ptr<T, D> &a, decltype(nullptr)) noexcept { return static_cast<bool>(a); }
    template <class T, class D>
    bool operator!=(decltype(nullptr), const unique_ptr<T, D> &a) noexcept { return static_cast<bool>(a); }

    namespace detail
    {
        template <class Type>
        struct _unique_if
        {
            using _Single = unique_ptr<Type>;
        };
        template <class Type>
        struct _unique_if<Type[]>
        {
            using _Unbounded_array = unique_ptr<Type[]>;
        };
        template <class Type, size_t _Nx>
        struct _unique_if<Type[_Nx]>
        {
            using _Bounded_array = void;
        };
    } // namespace detail

    template <class Type, class... _Args>
    typename detail::_unique_if<Type>::_Single make_unique(_Args &&...args)
    {
        return unique_ptr<Type>(new Type(::SFTL::forward<_Args>(args)...));
    }

    template <class Type>
    typename detail::_unique_if<Type>::_Unbounded_array make_unique(size_t n)
    {
        using _Elem = remove_extent_t<Type>;
        return unique_ptr<Type>(new _Elem[n]());
    }

    template <class Type, class... _Args>
    typename detail::_unique_if<Type>::_Bounded_array make_unique(_Args &&...) = delete;

    template <class Type>
    typename detail::_unique_if<Type>::_Single make_unique_for_overwrite()
    {
        return unique_ptr<Type>(new Type);
    }

    template <class Type>
    typename detail::_unique_if<Type>::_Unbounded_array make_unique_for_overwrite(size_t n)
    {
        using _Elem = remove_extent_t<Type>;
        return unique_ptr<Type>(new _Elem[n]);
    }

    template <class Type>
    class shared_ptr;
    template <class Type>
    class weak_ptr;

    namespace detail
    {
        class _sp_control_block_base
        {
        public:
            _sp_control_block_base() noexcept : _shared_count(1), _weak_count(1) {}
            virtual ~_sp_control_block_base() = default;

            _sp_control_block_base(const _sp_control_block_base &) = delete;
            _sp_control_block_base &operator=(const _sp_control_block_base &) = delete;

            void add_shared_ref() noexcept
            {
                _shared_count.fetch_add(1, memory_order_relaxed);
            }

            void add_weak_ref() noexcept
            {
                _weak_count.fetch_add(1, memory_order_relaxed);
            }

            void release_shared() noexcept
            {
                if (_shared_count.fetch_sub(1, memory_order_acq_rel) == 1)
                {
                    _destroy_resource();
                    release_weak();
                }
            }

            void release_weak() noexcept
            {
                if (_weak_count.fetch_sub(1, memory_order_acq_rel) == 1)
                    _destroy_self();
            }

            bool try_add_shared_ref() noexcept
            {
                long count = _shared_count.load(memory_order_relaxed);
                while (count != 0)
                {
                    if (_shared_count.compare_exchange_weak(
                            count, count + 1,
                            memory_order_acq_rel,
                            memory_order_relaxed))
                        return true;
                }
                return false;
            }

            long use_count() const noexcept
            {
                return _shared_count.load(memory_order_relaxed);
            }

        protected:
            virtual void _destroy_resource() noexcept = 0;
            virtual void _destroy_self() noexcept = 0;

        private:
            atomic<long> _shared_count;
            atomic<long> _weak_count;
        };

        template <class Type, class _Dx>
        class _sp_control_block_ptr final : public _sp_control_block_base
        {
        public:
            _sp_control_block_ptr(Type *p, _Dx d) noexcept : _ptr(p), _deleter(::SFTL::move(d)) {}

        protected:
            void _destroy_resource() noexcept override { _deleter(_ptr); }
            void _destroy_self() noexcept override { delete this; }

        private:
            Type *_ptr;
            _Dx _deleter;
        };

        template <class Type>
        class _sp_control_block_obj final : public _sp_control_block_base
        {
        public:
            template <class... _Args>
            explicit _sp_control_block_obj(_Args &&...args)
            {
                ::new (static_cast<void *>(&_storage)) Type(::SFTL::forward<_Args>(args)...);
            }

            Type *get() noexcept { return reinterpret_cast<Type *>(&_storage); }

        protected:
            void _destroy_resource() noexcept override { get()->~Type(); }
            void _destroy_self() noexcept override { delete this; }

        private:
            alignas(Type) unsigned char _storage[sizeof(Type)];
        };
    } // namespace detail

    template <class Type>
    class shared_ptr
    {
    public:
        using element_type = Type;

        constexpr shared_ptr() noexcept : _ptr(nullptr), _ctrl(nullptr) {}
        constexpr shared_ptr(decltype(nullptr)) noexcept : _ptr(nullptr), _ctrl(nullptr) {}

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        explicit shared_ptr(_Uty *p)
            : _ptr(p),
              _ctrl(new detail::_sp_control_block_ptr<_Uty, default_delete<_Uty>>(p, default_delete<_Uty>()))
        {
        }

        template <class _Uty, class _Dx, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        shared_ptr(_Uty *p, _Dx d)
            : _ptr(p), _ctrl(new detail::_sp_control_block_ptr<_Uty, _Dx>(p, ::SFTL::move(d)))
        {
        }

        shared_ptr(const shared_ptr &other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        shared_ptr(const shared_ptr<_Uty> &other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        shared_ptr(shared_ptr &&other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            other._ptr = nullptr;
            other._ctrl = nullptr;
        }

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        shared_ptr(shared_ptr<_Uty> &&other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            other._ptr = nullptr;
            other._ctrl = nullptr;
        }

        template <class _Uty>
        shared_ptr(const shared_ptr<_Uty> &other, Type *aliased) noexcept
            : _ptr(aliased), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        template <class _Uty, class _Dx, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        explicit shared_ptr(unique_ptr<_Uty, _Dx> &&up)
            : _ptr(up.get()),
              _ctrl(up.get() ? new detail::_sp_control_block_ptr<_Uty, _Dx>(up.get(), ::SFTL::move(up.get_deleter()))
                             : nullptr)
        {
            up.release();
        }

        ~shared_ptr() { _release(); }

        shared_ptr &operator=(const shared_ptr &other) noexcept
        {
            shared_ptr(other).swap(*this);
            return *this;
        }

        shared_ptr &operator=(shared_ptr &&other) noexcept
        {
            shared_ptr(::SFTL::move(other)).swap(*this);
            return *this;
        }

        shared_ptr &operator=(decltype(nullptr)) noexcept
        {
            shared_ptr().swap(*this);
            return *this;
        }

        void reset() noexcept { shared_ptr().swap(*this); }

        template <class _Uty>
        void reset(_Uty *p) { shared_ptr(p).swap(*this); }

        template <class _Uty, class _Dx>
        void reset(_Uty *p, _Dx d) { shared_ptr(p, ::SFTL::move(d)).swap(*this); }

        void swap(shared_ptr &other) noexcept
        {
            Type *tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;

            detail::_sp_control_block_base *tmp_c = _ctrl;
            _ctrl = other._ctrl;
            other._ctrl = tmp_c;
        }

        Type *get() const noexcept { return _ptr; }
        add_lvalue_reference_t<Type> operator*() const { return *_ptr; }
        Type *operator->() const noexcept { return _ptr; }

        long use_count() const noexcept { return _ctrl ? _ctrl->use_count() : 0; }
        bool unique() const noexcept { return use_count() == 1; }
        explicit operator bool() const noexcept { return _ptr != nullptr; }

    private:
        void _release() noexcept
        {
            if (_ctrl)
                _ctrl->release_shared();
        }

        template <class _Uty>
        friend class shared_ptr;
        template <class _Uty>
        friend class weak_ptr;

        template <class _Uty, class... _Args>
        friend shared_ptr<_Uty> make_shared(_Args &&...);

        Type *_ptr;
        detail::_sp_control_block_base *_ctrl;
    };

    template <class Type>
    class weak_ptr
    {
    public:
        constexpr weak_ptr() noexcept : _ptr(nullptr), _ctrl(nullptr) {}

        weak_ptr(const weak_ptr &other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        weak_ptr(const weak_ptr<_Uty> &other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        template <class _Uty, enable_if_t<is_convertible_v<_Uty *, Type *>, int> = 0>
        weak_ptr(const shared_ptr<_Uty> &sp) noexcept : _ptr(sp._ptr), _ctrl(sp._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        weak_ptr(weak_ptr &&other) noexcept : _ptr(other._ptr), _ctrl(other._ctrl)
        {
            other._ptr = nullptr;
            other._ctrl = nullptr;
        }

        ~weak_ptr() { _release(); }

        weak_ptr &operator=(const weak_ptr &other) noexcept
        {
            weak_ptr(other).swap(*this);
            return *this;
        }

        weak_ptr &operator=(weak_ptr &&other) noexcept
        {
            weak_ptr(::SFTL::move(other)).swap(*this);
            return *this;
        }

        template <class _Uty>
        weak_ptr &operator=(const shared_ptr<_Uty> &sp) noexcept
        {
            weak_ptr(sp).swap(*this);
            return *this;
        }

        void reset() noexcept { weak_ptr().swap(*this); }

        void swap(weak_ptr &other) noexcept
        {
            Type *tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;
            detail::_sp_control_block_base *tmp_c = _ctrl;
            _ctrl = other._ctrl;
            other._ctrl = tmp_c;
        }

        long use_count() const noexcept { return _ctrl ? _ctrl->use_count() : 0; }
        bool expired() const noexcept { return use_count() == 0; }

        shared_ptr<Type> lock() const noexcept
        {
            shared_ptr<Type> result;
            if (_ctrl && _ctrl->try_add_shared_ref())
            {
                result._ptr = _ptr;
                result._ctrl = _ctrl;
            }
            return result;
        }

    private:
        void _release() noexcept
        {
            if (_ctrl)
                _ctrl->release_weak();
        }

        template <class _Uty>
        friend class weak_ptr;
        template <class _Uty>
        friend class shared_ptr;

        Type *_ptr;
        detail::_sp_control_block_base *_ctrl;
    };

    template <class Type, class... _Args>
    shared_ptr<Type> make_shared(_Args &&...args)
    {
        auto *ctrl = new detail::_sp_control_block_obj<Type>(::SFTL::forward<_Args>(args)...);
        shared_ptr<Type> sp;
        sp._ptr = ctrl->get();
        sp._ctrl = ctrl;
        return sp;
    }

    template <class T1, class T2>
    bool operator==(const shared_ptr<T1> &a, const shared_ptr<T2> &b) noexcept { return a.get() == b.get(); }
    template <class T1, class T2>
    bool operator!=(const shared_ptr<T1> &a, const shared_ptr<T2> &b) noexcept { return a.get() != b.get(); }
    template <class T>
    bool operator==(const shared_ptr<T> &a, decltype(nullptr)) noexcept { return !a; }
    template <class T>
    bool operator==(decltype(nullptr), const shared_ptr<T> &a) noexcept { return !a; }
    template <class T>
    bool operator!=(const shared_ptr<T> &a, decltype(nullptr)) noexcept { return static_cast<bool>(a); }
    template <class T>
    bool operator!=(decltype(nullptr), const shared_ptr<T> &a) noexcept { return static_cast<bool>(a); }

} // namespace SFTL