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

namespace SFTL
{
    // just the worst implementation of <memory> ever

    template<class Type>
    struct default_delete
    {
        constexpr default_delete() noexcept = default;

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        constexpr default_delete(const default_delete<UniqueType> &) noexcept
        {
        }

        constexpr void operator()(Type *Pointer) const noexcept
        {
            static_assert(0 < sizeof(Type), "can't delete an incomplete type");
            delete Pointer;
        }
    };

    template<class Type>
    struct default_delete<Type[]>
    {
        constexpr default_delete() noexcept = default;

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        constexpr default_delete(const default_delete<UniqueType[]> &) noexcept
        {
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        constexpr void operator()(UniqueType *Pointer) const noexcept
        {
            static_assert(0 < sizeof(UniqueType), "can't delete an incomplete type");
            delete[] Pointer;
        }
    };

    namespace detail
    {
        template<class DelType, class Type, typename = void>
        struct _unique_pointer_pointer
        {
            using type = Type *;
        };
        template<class DelType, class Type>
        struct _unique_pointer_pointer<DelType, Type, void_t<typename remove_reference_t<DelType>::pointer>>
        {
            using type = typename remove_reference_t<DelType>::pointer;
        };
    } // namespace detail

    template<class Type, class DelType = default_delete<Type>>
    class unique_pointer
    {
        static_assert(!is_reference_v<DelType>, "SFTL::unique_pointer does not support reference deleters");

    public:
        using element_type = Type;
        using deleter_type = DelType;
        using pointer      = typename detail::_unique_pointer_pointer<DelType, Type>::type;

        constexpr unique_pointer() noexcept : Pointer(nullptr), _deleter() {}
        constexpr unique_pointer(decltype(nullptr)) noexcept : Pointer(nullptr), _deleter() {}

        explicit unique_pointer(pointer p) noexcept : Pointer(p), _deleter() {}
        unique_pointer(pointer p, const DelType &d) noexcept : Pointer(p), _deleter(d) {}
        unique_pointer(pointer p, DelType &&d) noexcept : Pointer(p), _deleter(::SFTL::move(d)) {}

        unique_pointer(const unique_pointer &)            = delete;
        unique_pointer &operator=(const unique_pointer &) = delete;

        unique_pointer(unique_pointer &&other) noexcept : Pointer(other.Pointer), _deleter(::SFTL::move(other._deleter))
        {
            other.Pointer = nullptr;
        }

        template<class UniqueType, class _Ex,
                 enable_if_t<!is_array_v<UniqueType> &&
                                     is_convertible_v<typename unique_pointer<UniqueType, _Ex>::pointer, pointer> &&
                                     is_convertible_v<_Ex, DelType>,
                             int> = 0>
        explicit unique_pointer(unique_pointer<UniqueType, _Ex> &&other) noexcept :
            Pointer(other.release()), _deleter(::SFTL::forward<_Ex>(other.get_deleter()))
        {
        }

        unique_pointer &operator=(unique_pointer &&other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
                _deleter = ::SFTL::move(other._deleter);
            }
            return *this;
        }

        unique_pointer &operator=(decltype(nullptr)) noexcept
        {
            reset();
            return *this;
        }

        ~unique_pointer()
        {
            if (Pointer)
                _deleter(Pointer);
        }

        pointer release() noexcept
        {
            pointer old = Pointer;
            Pointer     = nullptr;
            return old;
        }

        void reset(pointer p = pointer()) noexcept
        {
            pointer old = Pointer;
            Pointer     = p;
            if (old)
                _deleter(old);
        }

        void swap(unique_pointer &other) noexcept
        {
            pointer tmp_p = Pointer;
            Pointer       = other.Pointer;
            other.Pointer = tmp_p;

            DelType tmp_d  = ::SFTL::move(_deleter);
            _deleter       = ::SFTL::move(other._deleter);
            other._deleter = ::SFTL::move(tmp_d);
        }

        pointer get() const noexcept { return Pointer; }
        DelType &get_deleter() noexcept { return _deleter; }
        const DelType &get_deleter() const noexcept { return _deleter; }

        explicit operator bool() const noexcept { return Pointer != nullptr; }

        add_lvalue_reference_t<Type> operator*() const { return *Pointer; }
        pointer operator->() const noexcept { return Pointer; }

    private:
        pointer Pointer;
        DelType _deleter;
    };

    template<class Type, class DelType>
    class unique_pointer<Type[], DelType>
    {
    public:
        using element_type = Type;
        using deleter_type = DelType;
        using pointer      = typename detail::_unique_pointer_pointer<DelType, Type>::type;

        constexpr unique_pointer() noexcept : Pointer(nullptr), _deleter() {}
        constexpr unique_pointer(decltype(nullptr)) noexcept : Pointer(nullptr), _deleter() {}

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        explicit unique_pointer(UniqueType p) noexcept : Pointer(p), _deleter()
        {
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        unique_pointer(UniqueType p, const DelType &d) noexcept : Pointer(p), _deleter(d)
        {
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        unique_pointer(UniqueType p, DelType &&d) noexcept : Pointer(p), _deleter(::SFTL::move(d))
        {
        }

        unique_pointer(const unique_pointer &)            = delete;
        unique_pointer &operator=(const unique_pointer &) = delete;

        unique_pointer(unique_pointer &&other) noexcept : Pointer(other.Pointer), _deleter(::SFTL::move(other._deleter))
        {
            other.Pointer = nullptr;
        }

        unique_pointer &operator=(unique_pointer &&other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
                _deleter = ::SFTL::move(other._deleter);
            }
            return *this;
        }

        unique_pointer &operator=(decltype(nullptr)) noexcept
        {
            reset();
            return *this;
        }

        ~unique_pointer()
        {
            if (Pointer)
                _deleter(Pointer);
        }

        pointer release() noexcept
        {
            pointer old = Pointer;
            Pointer     = nullptr;
            return old;
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType (*)[], Type (*)[]>, int> = 0>
        void reset(UniqueType p) noexcept
        {
            pointer old = Pointer;
            Pointer     = p;
            if (old)
                _deleter(old);
        }

        void reset(decltype(nullptr) = nullptr) noexcept
        {
            pointer old = Pointer;
            Pointer     = nullptr;
            if (old)
                _deleter(old);
        }

        void swap(unique_pointer &other) noexcept
        {
            pointer tmp_p  = Pointer;
            Pointer        = other.Pointer;
            other.Pointer  = tmp_p;
            DelType tmp_d  = ::SFTL::move(_deleter);
            _deleter       = ::SFTL::move(other._deleter);
            other._deleter = ::SFTL::move(tmp_d);
        }

        pointer get() const noexcept { return Pointer; }
        DelType &get_deleter() noexcept { return _deleter; }
        const DelType &get_deleter() const noexcept { return _deleter; }

        explicit operator bool() const noexcept { return Pointer != nullptr; }
        Type &operator[](size_type i) const { return Pointer[i]; }

    private:
        pointer Pointer;
        DelType _deleter;
    };

    template<class T1, class D1, class T2, class D2>
    bool operator==(const unique_pointer<T1, D1> &a, const unique_pointer<T2, D2> &b)
    {
        return a.get() == b.get();
    }
    template<class T1, class D1, class T2, class D2>
    bool operator!=(const unique_pointer<T1, D1> &a, const unique_pointer<T2, D2> &b)
    {
        return a.get() != b.get();
    }
    template<class T, class D>
    bool operator==(const unique_pointer<T, D> &a, decltype(nullptr)) noexcept
    {
        return !a;
    }
    template<class T, class D>
    bool operator==(decltype(nullptr), const unique_pointer<T, D> &a) noexcept
    {
        return !a;
    }
    template<class T, class D>
    bool operator!=(const unique_pointer<T, D> &a, decltype(nullptr)) noexcept
    {
        return static_cast<bool>(a);
    }
    template<class T, class D>
    bool operator!=(decltype(nullptr), const unique_pointer<T, D> &a) noexcept
    {
        return static_cast<bool>(a);
    }

    namespace detail
    {
        template<class Type>
        struct _unique_if
        {
            using Single = unique_pointer<Type>;
        };
        template<class Type>
        struct _unique_if<Type[]>
        {
            using UnboundedArray = unique_pointer<Type[]>;
        };
        template<class Type, size_type st>
        struct _unique_if<Type[st]>
        {
            using BoundedArray = void;
        };
    } // namespace detail

    template<class Type, class... Arguments>
    typename detail::_unique_if<Type>::Single make_unique(Arguments &&...args)
    {
        return unique_pointer<Type>(new Type(::SFTL::forward<Arguments>(args)...));
    }

    template<class Type>
    typename detail::_unique_if<Type>::UnboundedArray make_unique(size_type n)
    {
        using Element = remove_extent_t<Type>;
        return unique_pointer<Type>(new Element[n]());
    }

    template<class Type, class... Arguments>
    typename detail::_unique_if<Type>::BoundedArray make_unique(Arguments &&...) = delete;

    template<class Type>
    typename detail::_unique_if<Type>::Single make_unique_for_overwrite()
    {
        return unique_pointer<Type>(new Type);
    }

    template<class Type>
    typename detail::_unique_if<Type>::UnboundedArray make_unique_for_overwrite(size_type n)
    {
        using Element = remove_extent_t<Type>;
        return unique_pointer<Type>(new Element[n]);
    }

    template<class Type>
    class sharedPointer;
    template<class Type>
    class weakPointer;

    namespace detail
    {
        class _sp_control_block_base
        {
        public:
            _sp_control_block_base() noexcept : _shared_count(1), _weak_count(1) {}
            virtual ~_sp_control_block_base() = default;

            _sp_control_block_base(const _sp_control_block_base &)            = delete;
            _sp_control_block_base &operator=(const _sp_control_block_base &) = delete;

            void add_shared_ref() noexcept { _shared_count.fetch_add(1, memory_order_relaxed); }

            void add_weak_ref() noexcept { _weak_count.fetch_add(1, memory_order_relaxed); }

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
                    if (_shared_count.compare_exchange_weak(count, count + 1, memory_order_acq_rel,
                                                            memory_order_relaxed))
                        return true;
                }
                return false;
            }

            long use_count() const noexcept { return _shared_count.load(memory_order_relaxed); }

        protected:
            virtual void _destroy_resource() noexcept = 0;
            virtual void _destroy_self() noexcept     = 0;

        private:
            atomic<long> _shared_count;
            atomic<long> _weak_count;
        };

        template<class Type, class DelType>
        class _sp_control_blockPointer final : public _sp_control_block_base
        {
        public:
            _sp_control_blockPointer(Type *p, DelType d) noexcept : Pointer(p), _deleter(::SFTL::move(d)) {}

        protected:
            void _destroy_resource() noexcept override { _deleter(Pointer); }
            void _destroy_self() noexcept override { delete this; }

        private:
            Type *Pointer;
            DelType _deleter;
        };

        template<class Type>
        class _sp_control_block_obj final : public _sp_control_block_base
        {
        public:
            template<class... Arguments>
            explicit _sp_control_block_obj(Arguments &&...args)
            {
                ::new (static_cast<void *>(&_storage)) Type(::SFTL::forward<Arguments>(args)...);
            }

            Type *get() noexcept { return reinterpret_cast<Type *>(&_storage); }

        protected:
            void _destroy_resource() noexcept override { get()->~Type(); }
            void _destroy_self() noexcept override { delete this; }

        private:
            alignas(Type) unsigned char _storage[sizeof(Type)];
        };
    } // namespace detail

    template<class Type>
    class sharedPointer
    {
    public:
        using element_type = Type;

        constexpr sharedPointer() noexcept : Pointer(nullptr), _ctrl(nullptr) {}
        constexpr sharedPointer(decltype(nullptr)) noexcept : Pointer(nullptr), _ctrl(nullptr) {}

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        explicit sharedPointer(UniqueType *p) :
            Pointer(p), _ctrl(new detail::_sp_control_blockPointer<UniqueType, default_delete<UniqueType>>(
                                p, default_delete<UniqueType>()))
        {
        }

        template<class UniqueType, class DelType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        sharedPointer(UniqueType *p, DelType d) :
            Pointer(p), _ctrl(new detail::_sp_control_blockPointer<UniqueType, DelType>(p, ::SFTL::move(d)))
        {
        }

        sharedPointer(const sharedPointer &other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        sharedPointer(const sharedPointer<UniqueType> &other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        sharedPointer(sharedPointer &&other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            other.Pointer = nullptr;
            other._ctrl   = nullptr;
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        sharedPointer(sharedPointer<UniqueType> &&other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            other.Pointer = nullptr;
            other._ctrl   = nullptr;
        }

        template<class UniqueType>
        sharedPointer(const sharedPointer<UniqueType> &other, Type *aliased) noexcept :
            Pointer(aliased), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_shared_ref();
        }

        template<class UniqueType, class DelType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        explicit sharedPointer(unique_pointer<UniqueType, DelType> &&up) :
            Pointer(up.get()), _ctrl(up.get() ? new detail::_sp_control_blockPointer<UniqueType, DelType>(
                                                        up.get(), ::SFTL::move(up.get_deleter()))
                                              : nullptr)
        {
            up.release();
        }

        ~sharedPointer() { _release(); }

        sharedPointer &operator=(const sharedPointer &other) noexcept
        {
            sharedPointer(other).swap(*this);
            return *this;
        }

        sharedPointer &operator=(sharedPointer &&other) noexcept
        {
            sharedPointer(::SFTL::move(other)).swap(*this);
            return *this;
        }

        sharedPointer &operator=(decltype(nullptr)) noexcept
        {
            sharedPointer().swap(*this);
            return *this;
        }

        void reset() noexcept { sharedPointer().swap(*this); }

        template<class UniqueType>
        void reset(UniqueType *p)
        {
            sharedPointer(p).swap(*this);
        }

        template<class UniqueType, class DelType>
        void reset(UniqueType *p, DelType d)
        {
            sharedPointer(p, ::SFTL::move(d)).swap(*this);
        }

        void swap(sharedPointer &other) noexcept
        {
            Type *tmp_p   = Pointer;
            Pointer       = other.Pointer;
            other.Pointer = tmp_p;

            detail::_sp_control_block_base *tmp_c = _ctrl;
            _ctrl                                 = other._ctrl;
            other._ctrl                           = tmp_c;
        }

        Type *get() const noexcept { return Pointer; }
        add_lvalue_reference_t<Type> operator*() const { return *Pointer; }
        Type *operator->() const noexcept { return Pointer; }

        [[nodiscard]] long use_count() const noexcept { return _ctrl ? _ctrl->use_count() : 0; }
        [[nodiscard]] bool unique() const noexcept { return use_count() == 1; }
        explicit operator bool() const noexcept { return Pointer != nullptr; }

    private:
        void _release() noexcept
        {
            if (_ctrl)
                _ctrl->release_shared();
        }

        template<class UniqueType>
        friend class sharedPointer;
        template<class UniqueType>
        friend class weakPointer;

        template<class UniqueType, class... Arguments>
        friend sharedPointer<UniqueType> make_shared(Arguments &&...);

        Type *Pointer;
        detail::_sp_control_block_base *_ctrl;
    };

    template<class Type>
    class weakPointer
    {
    public:
        constexpr weakPointer() noexcept : Pointer(nullptr), _ctrl(nullptr) {}

        weakPointer(const weakPointer &other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        weakPointer(const weakPointer<UniqueType> &other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        template<class UniqueType, enable_if_t<is_convertible_v<UniqueType *, Type *>, int> = 0>
        weakPointer(const sharedPointer<UniqueType> &sp) noexcept : Pointer(sp.Pointer), _ctrl(sp._ctrl)
        {
            if (_ctrl)
                _ctrl->add_weak_ref();
        }

        weakPointer(weakPointer &&other) noexcept : Pointer(other.Pointer), _ctrl(other._ctrl)
        {
            other.Pointer = nullptr;
            other._ctrl   = nullptr;
        }

        ~weakPointer() { _release(); }

        weakPointer &operator=(const weakPointer &other) noexcept
        {
            weakPointer(other).swap(*this);
            return *this;
        }

        weakPointer &operator=(weakPointer &&other) noexcept
        {
            weakPointer(::SFTL::move(other)).swap(*this);
            return *this;
        }

        template<class UniqueType>
        weakPointer &operator=(const sharedPointer<UniqueType> &sp) noexcept
        {
            weakPointer(sp).swap(*this);
            return *this;
        }

        void reset() noexcept { weakPointer().swap(*this); }

        void swap(weakPointer &other) noexcept
        {
            Type *tmp_p                           = Pointer;
            Pointer                               = other.Pointer;
            other.Pointer                         = tmp_p;
            detail::_sp_control_block_base *tmp_c = _ctrl;
            _ctrl                                 = other._ctrl;
            other._ctrl                           = tmp_c;
        }

        [[nodiscard]] long use_count() const noexcept { return _ctrl ? _ctrl->use_count() : 0; }
        [[nodiscard]] bool expired() const noexcept { return use_count() == 0; }

        sharedPointer<Type> lock() const noexcept
        {
            sharedPointer<Type> result;
            if (_ctrl && _ctrl->try_add_shared_ref())
            {
                result.Pointer = Pointer;
                result._ctrl   = _ctrl;
            }
            return result;
        }

    private:
        void _release() noexcept
        {
            if (_ctrl)
                _ctrl->release_weak();
        }

        template<class UniqueType>
        friend class weakPointer;
        template<class UniqueType>
        friend class sharedPointer;

        Type *Pointer;
        detail::_sp_control_block_base *_ctrl;
    };

    template<class Type, class... Arguments>
    sharedPointer<Type> make_shared(Arguments &&...args)
    {
        auto *ctrl = new detail::_sp_control_block_obj<Type>(::SFTL::forward<Arguments>(args)...);
        sharedPointer<Type> sp;
        sp.Pointer = ctrl->get();
        sp._ctrl   = ctrl;
        return sp;
    }

    template<class T1, class T2>
    bool operator==(const sharedPointer<T1> &a, const sharedPointer<T2> &b) noexcept
    {
        return a.get() == b.get();
    }
    template<class T1, class T2>
    bool operator!=(const sharedPointer<T1> &a, const sharedPointer<T2> &b) noexcept
    {
        return a.get() != b.get();
    }
    template<class T>
    bool operator==(const sharedPointer<T> &a, decltype(nullptr)) noexcept
    {
        return !a;
    }
    template<class T>
    bool operator==(decltype(nullptr), const sharedPointer<T> &a) noexcept
    {
        return !a;
    }
    template<class T>
    bool operator!=(const sharedPointer<T> &a, decltype(nullptr)) noexcept
    {
        return static_cast<bool>(a);
    }
    template<class T>
    bool operator!=(decltype(nullptr), const sharedPointer<T> &a) noexcept
    {
        return static_cast<bool>(a);
    }

    // todo: other pointer types

} // namespace SFTL
