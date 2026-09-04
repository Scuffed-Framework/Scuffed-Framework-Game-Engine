#pragma once

#include <tuple>
#include "Invoke.hpp"
#include "TypeTraits.hpp"

namespace SFTL
{
    template<typename Signature>
    class function;

    template<typename R, typename... Args>
    class function<R(Args...)>
    {
    private:
        static constexpr size_t SmallBufferSize = 32; // Cache line friendly
        static constexpr size_t Alignment       = alignof(max_align_t);

        struct CallableBase
        {
            virtual ~CallableBase()                         = default;
            virtual R Call(Args... args)                    = 0;
            virtual CallableBase *Clone(void *buffer) const = 0;
            virtual void Destroy() noexcept                 = 0;
            virtual bool IsNull() const noexcept            = 0;
        };

        template<typename T>
        struct Callable : CallableBase
        {
            T callable;

            template<typename U>
            Callable(U &&callable) : Callable(Forward<U>(callable))
            {
            }

            R Call(Args... args) override { return Invoke(callable, args...); }

            CallableBase *Clone(void *buffer) const override
            {
                if (sizeof(Callable<T>) <= SmallBufferSize)
                {
                    return new (buffer) Callable<T>(callable);
                }
                return new Callable<T>(callable);
            }

            void Destroy() noexcept override { this->~Callable(); }

            bool IsNull() const noexcept override { return false; }
        };

        alignas(Alignment) unsigned char SmallBuffer[SmallBufferSize];
        CallableBase *Storage;
        bool IsSmall;

    public:
        function() noexcept : SmallBuffer{}, Storage(nullptr), IsSmall(false) {}

        template<typename T, typename = enable_if<!is_same<remove_cvref_t<T>, function>::Value>>
        function(T &&callable) : Storage(nullptr), IsSmall(false)
        {
            using DecayedT = remove_cvref_t<T>;
            static_assert(std::is_invocable<T, Args...>::Value, "Callable must be invocable with Args");
            static_assert(is_same<invoke_result_t<T, Args...>, R>::Value || is_void<R>::Value,
                          "Callable return type must match function signature");

            if constexpr (sizeof(Callable<DecayedT>) <= SmallBufferSize && alignof(Callable<DecayedT>) <= Alignment)
            {
                IsSmall = true;
                Storage = new (SmallBuffer) Callable<DecayedT>(Forward<T>(callable));
            } else
            {
                IsSmall = false;
                Storage = new Callable<DecayedT>(Forward<T>(callable));
            }
        }

        function(const function &other) : IsSmall(other.IsSmall)
        {
            if (other.Storage)
            {
                if (IsSmall)
                {
                    Storage = other.Storage->Clone(SmallBuffer);
                } else
                {
                    Storage = other.Storage->Clone(nullptr);
                }
            } else
            {
                Storage = nullptr;
            }
        }

        function(function &&other) noexcept : Storage(other.Storage), IsSmall(other.IsSmall)
        {
            if (IsSmall && other.Storage)
            {
                Storage = other.Storage->Clone(SmallBuffer);
                other.Storage->Destroy();
                other.Storage = nullptr;
            } else
            {
                other.Storage = nullptr;
            }
        }

        ~function()
        {
            if (Storage)
            {
                if (IsSmall)
                {
                    Storage->Destroy();
                } else
                {
                    delete Storage;
                }
            }
        }

        function &operator=(const function &other)
        {
            if (this != &other)
            {
                function temp(other);
                Swap(temp);
            }
            return *this;
        }

        function &operator=(function &&other) noexcept
        {
            if (this != &other)
            {
                function temp(Move(other));
                Swap(temp);
            }
            return *this;
        }

        template<typename T>
        function &operator=(T &&callable)
        {
            function temp(Forward<T>(callable));
            Swap(temp);
            return *this;
        }

        R operator()(Args... args) const { return Storage->Call(args...); }

        explicit operator bool() const noexcept { return Storage != nullptr; }

        bool IsNull() const noexcept { return Storage == nullptr; }

        void Swap(function &other) noexcept
        {
            // Manual swap for SBO optimization
            if (IsSmall && other.IsSmall)
            {
                if (Storage)
                {
                    unsigned char tempBuffer[SmallBufferSize];
                    Storage = Storage->Clone(tempBuffer);
                    Storage->Destroy();
                }

                // Move other to this
                if (other.Storage)
                {
                    other.Storage = other.Storage->Clone(SmallBuffer);
                    other.Storage->Destroy();
                }

                // Move temp to other
                if (Storage)
                {
                    other.Storage = Storage->Clone(other.SmallBuffer);
                    other.Storage->Destroy();
                }
            } else
            {
                // Not both small - simple pointer swap
                Swap(Storage, other.Storage);
                Swap(IsSmall, other.IsSmall);
            }
        }

        void Reset() noexcept
        {
            function empty;
            Swap(empty);
        }
    };

    template<typename F, typename G>
    class ComposedFunction
    {
        F First;
        G Second;

    public:
        ComposedFunction(F &&first, G &&second) : First(Forward<F>(first)), Second(Forward<G>(second)) {}

        template<typename... Args>
        auto operator()(Args &&...args) const -> invoke_result_t<F, invoke_result_t<G, Args...>>
        {
            return Invoke(First, Invoke(Second, Forward<Args>(args)...));
        }
    };

    template<typename F, typename G>
    auto Compose(F &&f, G &&g)
    {
        return ComposedFunction<remove_cvref<F>, remove_cvref<G>>(Forward<F>(f), Forward<G>(g));
    }

    template<typename F, typename... BoundArgs>
    class BoundFunction
    {
        F Callable;
        std::tuple<BoundArgs...> Bound;

        template<size_t... Is, typename... CallArgs>
        auto CallImpl(std::tuple<BoundArgs...> &bound, std::index_sequence<Is...>, CallArgs &&...args) const
                -> invoke_result_t<F, BoundArgs..., CallArgs...>
        {
            return Invoke(Callable, std::get<Is>(bound)..., std::forward<CallArgs>(args)...);
        }

    public:
        template<typename... U>
        explicit BoundFunction(F &&f, U &&...args) : Callable(std::forward<F>(f)), Bound(std::forward<U>(args)...)
        {
        }

        template<typename... CallArgs>
        auto operator()(CallArgs &&...args) const -> invoke_result_t<F, BoundArgs..., CallArgs...>
        {
            return CallImpl(Bound, std::make_index_sequence<sizeof...(BoundArgs)>{}, std::forward<CallArgs>(args)...);
        }
    };

    template<typename F, typename... Args>
    auto Bind(F &&f, Args &&...args)
    {
        return BoundFunction<remove_cvref<F>, remove_cvref<Args>...>(Forward<F>(f), Forward<Args>(args)...);
    }

    namespace Placeholders
    {
        template<int I>
        struct Placeholder
        {
        };

        template<typename T>
        struct IsPlaceholder : false_type
        {
        };

        template<int I>
        struct IsPlaceholder<Placeholder<I>> : true_type
        {
        };

        template<typename T>
        static constexpr bool IsPlaceholderV = IsPlaceholder<T>::Value;

        // Standard placeholders
        constexpr Placeholder<0> _1{};
        constexpr Placeholder<1> _2{};
        constexpr Placeholder<2> _3{};
        constexpr Placeholder<3> _4{};
        constexpr Placeholder<4> _5{};
        constexpr Placeholder<5> _6{};
        constexpr Placeholder<6> _7{};
        constexpr Placeholder<7> _8{};
        constexpr Placeholder<8> _9{};
    } // namespace Placeholders


    template<typename T>
    class ReferenceWrapper
    {
        T *Ptr;

    public:
        template<typename U>
        ReferenceWrapper(U &ref) noexcept : Ptr(AddressOf(ref))
        {
        }

        operator T &() const noexcept { return *Ptr; }
        T &Get() const noexcept { return *Ptr; }

        T *operator&() const noexcept { return Ptr; }
    };

    template<typename T>
    ReferenceWrapper<T> Ref(T &t) noexcept
    {
        return ReferenceWrapper<T>(t);
    }

    template<typename T>
    ReferenceWrapper<const T> Ref(const T &t) noexcept
    {
        return ReferenceWrapper<const T>(t);
    }

    template<typename T>
    void Swap(ReferenceWrapper<T> a, ReferenceWrapper<T> b) noexcept
    {
        Swap(a.Get(), b.Get());
    }

    struct Less
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) < Forward<U>(b))
        {
            return Forward<T>(a) < Forward<U>(b);
        }
    };

    struct Greater
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) > Forward<U>(b))
        {
            return Forward<T>(a) > Forward<U>(b);
        }
    };

    struct EqualTo
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) == Forward<U>(b))
        {
            return Forward<T>(a) == Forward<U>(b);
        }
    };

    struct NotEqualTo
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) != Forward<U>(b))
        {
            return Forward<T>(a) != Forward<U>(b);
        }
    };

    struct Plus
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) + Forward<U>(b))
        {
            return Forward<T>(a) + Forward<U>(b);
        }
    };

    struct Minus
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) - Forward<U>(b))
        {
            return Forward<T>(a) - Forward<U>(b);
        }
    };

    struct Multiplies
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) * Forward<U>(b))
        {
            return Forward<T>(a) * Forward<U>(b);
        }
    };

    struct Divides
    {
        template<typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> decltype(Forward<T>(a) / Forward<U>(b))
        {
            return Forward<T>(a) / Forward<U>(b);
        }
    };

    template<typename T>
    constexpr T Identity(T &&t) noexcept
    {
        return Forward<T>(t);
    }

    template<typename T>
    constexpr T Constant(const T &value) noexcept
    {
        return [&value]() -> const T & { return value; };
    }

} // namespace SFTL
