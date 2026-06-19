#pragma once
#include <typeinfo>
#include <cassert>
#include "TypeTraits.hpp"

namespace SFTL
{
    class Any
    {
        static constexpr size_t SboSize = 64;
        static constexpr size_t SboAlign = alignof(max_align_t);

        struct Ops
        {
            void (*destroy)(void *storage);
            void (*copy)(const void *src, void *dst);
            void (*move)(void *src, void *dst);
            const std::type_info *type;
        };

        template <typename T>
        static constexpr bool UsesSbo =
            sizeof(T) <= SboSize &&
            alignof(T) <= SboAlign &&
            is_nothrow_move_constructible_v<T>;

        template <typename T>
        static const Ops *GetOps()
        {
            static constexpr Ops ops{
                [](void *s)
                {
                    if constexpr (UsesSbo<T>)
                        reinterpret_cast<T *>(s)->~T();
                    else
                        delete *reinterpret_cast<T **>(s);
                },
                [](const void *src, void *dst)
                {
                    if constexpr (UsesSbo<T>)
                        new (dst) T(*reinterpret_cast<const T *>(src));
                    else
                        *reinterpret_cast<T **>(dst) = new T(**reinterpret_cast<const T *const *>(src));
                },
                [](void *src, void *dst)
                {
                    if constexpr (UsesSbo<T>)
                    {
                        new (dst) T(SFTL::move(*reinterpret_cast<T *>(src)));
                        reinterpret_cast<T *>(src)->~T();
                    }
                    else
                    {
                        *reinterpret_cast<T **>(dst) = *reinterpret_cast<T **>(src);
                        *reinterpret_cast<T **>(src) = nullptr;
                    }
                },
                &typeid(T)};
            return &ops;
        }

        alignas(SboAlign) char storage_[SboSize]{};
        const Ops *ops_ = nullptr;

    public:
        Any() = default;

        template <typename T, typename = enable_if_t<!is_same_v<remove_cvref_t<T>, Any>>>
        Any(T &&value)
        {
            using U = remove_cvref_t<T>;
            ops_ = GetOps<U>();
            if constexpr (UsesSbo<U>)
                new (storage_) U(SFTL::forward<T>(value));
            else
                *reinterpret_cast<U **>(storage_) = new U(SFTL::forward<T>(value));
        }

        Any(const Any &other)
        {
            if (other.ops_)
            {
                ops_ = other.ops_;
                ops_->copy(other.storage_, storage_);
            }
        }

        Any(Any &&other) noexcept
        {
            if (other.ops_)
            {
                ops_ = other.ops_;
                ops_->move(other.storage_, storage_);
                other.ops_ = nullptr;
            }
        }

        Any &operator=(const Any &other)
        {
            if (this != &other)
            {
                reset();
                if (other.ops_)
                {
                    ops_ = other.ops_;
                    ops_->copy(other.storage_, storage_);
                }
            }
            return *this;
        }

        Any &operator=(Any &&other) noexcept
        {
            if (this != &other)
            {
                reset();
                if (other.ops_)
                {
                    ops_ = other.ops_;
                    ops_->move(other.storage_, storage_);
                    other.ops_ = nullptr;
                }
            }
            return *this;
        }

        ~Any() { reset(); }

        void reset()
        {
            if (ops_)
            {
                ops_->destroy(storage_);
                ops_ = nullptr;
            }
        }

        bool HasValue() const noexcept { return ops_ != nullptr; }

        const std::type_info &Type() const noexcept
        {
            return ops_ ? *ops_->type : typeid(void);
        }

        template <typename T>
        bool Is() const noexcept
        {
            return ops_ && *ops_->type == typeid(T);
        }

        template <typename T>
        T &As()
        {
            assert(Is<T>() && "SFTL::Any bad cast");
            if constexpr (UsesSbo<T>)
                return *reinterpret_cast<T *>(storage_);
            else
                return **reinterpret_cast<T **>(storage_);
        }

        template <typename T>
        const T &As() const
        {
            assert(Is<T>() && "SFTL::Any bad cast");
            if constexpr (UsesSbo<T>)
                return *reinterpret_cast<const T *>(storage_);
            else
                return **reinterpret_cast<const T **>(storage_);
        }

        template <typename T>
        T *TryAs() noexcept
        {
            if (!Is<T>())
                return nullptr;
            if constexpr (UsesSbo<T>)
                return reinterpret_cast<T *>(storage_);
            else
                return *reinterpret_cast<T **>(storage_);
        }

        template <typename T>
        const T *TryAs() const noexcept
        {
            if (!Is<T>())
                return nullptr;
            if constexpr (UsesSbo<T>)
                return reinterpret_cast<const T *>(storage_);
            else
                return *reinterpret_cast<const T **>(storage_);
        }
    };
}