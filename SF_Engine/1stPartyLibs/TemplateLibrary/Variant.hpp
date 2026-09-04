#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <new>
#include <utility>
#include "TypeTraits.hpp"

namespace SFTL
{
    inline constexpr size_type variant_npos = static_cast<size_type>(-1);

    template<size_type I, typename... Types>
    struct NthType;

    template<size_type I, typename First, typename... Rest>
    struct NthType<I, First, Rest...> : NthType<I - 1, Rest...>
    {
    };

    template<typename First, typename... Rest>
    struct NthType<0, First, Rest...>
    {
        using type = First;
    };

    template<typename... Types>
    class variant;

    template<typename Variant>
    struct variant_size;

    template<typename... Types>
    struct variant_size<variant<Types...>> : integral_constant<size_type, sizeof...(Types)>
    {
    };

    template<typename Variant>
    inline constexpr size_type variant_size_v = variant_size<Variant>::value;

    template<size_type I, typename Variant>
    struct variant_alternative;

    template<size_type I, typename... Types>
    struct variant_alternative<I, variant<Types...>>
    {
        static_assert(I < sizeof...(Types), "alternative index out of range");
        using type = typename NthType<I, Types...>::type;
    };

    template<size_type I, typename Variant>
    using variant_alternative_t = typename variant_alternative<I, Variant>::type;

    namespace Detail::VariantImpl
    {
        template<bool TrivialDtor, typename... Types>
        union StorageImpl
        {
            constexpr StorageImpl() noexcept {}
        };

        template<typename First, typename... Rest>
        union StorageImpl<true, First, Rest...>
        {
            constexpr StorageImpl() noexcept : _rest() {}

            template<typename... Args>
            constexpr StorageImpl(in_place_index_t<0>, Args &&...args) : _first(forward<Args>(args)...)
            {
            }

            template<size_type I, typename... Args>
            constexpr StorageImpl(in_place_index_t<I>, Args &&...args) :
                _rest(in_place_index<I - 1>, forward<Args>(args)...)
            {
            }

            First _first;
            StorageImpl<(is_trivially_destructible_v<Rest> && ...), Rest...> _rest;
        };

        template<typename First, typename... Rest>
        union StorageImpl<false, First, Rest...>
        {
            constexpr StorageImpl() noexcept : _rest() {}

            template<typename... Args>
            constexpr StorageImpl(in_place_index_t<0>, Args &&...args) : _first(forward<Args>(args)...)
            {
            }

            template<size_type I, typename... Args>
            constexpr StorageImpl(in_place_index_t<I>, Args &&...args) :
                _rest(in_place_index<I - 1>, forward<Args>(args)...)
            {
            }

            constexpr ~StorageImpl() {} // active member destroyed explicitly by the owning variant

            First _first;
            StorageImpl<(is_trivially_destructible_v<Rest> && ...), Rest...> _rest;
        };

        template<typename... Types>
        using Storage = StorageImpl<(is_trivially_destructible_v<Types> && ...), Types...>;

        template<size_type I, typename U>
        constexpr decltype(auto) get(U &&u) noexcept
        {
            if constexpr (I == 0)
                return (forward<U>(u)._first);
            else
                return (VariantImpl::get<I - 1>(forward<U>(u)._rest));
        }

        // smallest unsigned integer that can index N alternatives
        template<size_type N>
        using index_type_for = conditional_t<N <= 0xFF, uint8, conditional_t<N <= 0xFFFF, uint16, uint32>>;

        template<typename... Types>
        struct Traits
        {
            static constexpr bool trivial_dtor        = (is_trivially_destructible_v<Types> && ...);
            static constexpr bool trivial_copy_ctor   = (is_trivially_copy_constructible_v<Types> && ...);
            static constexpr bool trivial_move_ctor   = (is_trivially_move_constructible_v<Types> && ...);
            static constexpr bool trivial_copy_assign = (is_trivially_copy_assignable_v<Types> && ...);
            static constexpr bool trivial_move_assign = (is_trivially_move_assignable_v<Types> && ...);
            static constexpr bool all_trivial         = trivial_dtor && trivial_copy_ctor && trivial_move_ctor &&
                                                trivial_copy_assign && trivial_move_assign;
            static constexpr bool nothrow_move_ctor = (is_nothrow_move_constructible_v<Types> && ...);
            static constexpr bool copyable          = (is_copy_constructible_v<Types> && ...);
            static constexpr bool moveable          = (is_move_constructible_v<Types> && ...);
            static constexpr bool nothrow_swappable = nothrow_move_ctor && (is_nothrow_swappable_v<Types> && ...);
        };

        template<bool AllTrivial, typename... Types>
        class Base
        {
        protected:
            using StorageT  = Storage<Types...>;
            using IndexType = index_type_for<sizeof...(Types)>;
            using Traits_   = Traits<Types...>;

            template<size_type I>
            using AltT = typename NthType<I, Types...>::type;

            StorageT _storage;
            IndexType _index;

            constexpr Base() noexcept(is_nothrow_default_constructible_v<AltT<0>>) :
                _storage(in_place_index<0>), _index(0)
            {
            }

            template<size_type I, typename... Args>
            constexpr explicit Base(in_place_index_t<I>, Args &&...args) :
                _storage(in_place_index<I>, forward<Args>(args)...), _index(static_cast<IndexType>(I))
            {
            }

            // ---- destroy ----
            template<size_type I>
            static void destroy_one(StorageT &s) noexcept
            {
                using T = AltT<I>;
                VariantImpl::get<I>(s).~T();
            }

            template<size_type... Is>
            static constexpr auto destroy_table(index_sequence<Is...>)
            {
                using Fn = void (*)(StorageT &) noexcept;
                return std::array<Fn, sizeof...(Is)>{&Base::destroy_one<Is>...};
            }

            void destroy() noexcept
            {
                static constexpr auto table = destroy_table(make_index_sequence<sizeof...(Types)>());
                table[_index](_storage);
            }

            // ---- copy-construct this->_storage from other (other's alt is active) ----
            template<size_type I>
            static void copy_ctor_one(StorageT &dst, const StorageT &src)
            {
                using T = AltT<I>;
                ::new (static_cast<void *>(addressof(VariantImpl::get<I>(dst)))) T(VariantImpl::get<I>(src));
            }

            template<size_type... Is>
            static constexpr auto copy_ctor_table(index_sequence<Is...>)
            {
                using Fn = void (*)(StorageT &, const StorageT &);
                return std::array<Fn, sizeof...(Is)>{&Base::copy_ctor_one<Is>...};
            }

            void copy_construct_from(const Base &other)
            {
                static constexpr auto table = copy_ctor_table(make_index_sequence<sizeof...(Types)>());
                table[other._index](_storage, other._storage);
                _index = other._index;
            }

            // ---- move-construct ----
            template<size_type I>
            static void move_ctor_one(StorageT &dst, StorageT &src)
            {
                using T = AltT<I>;
                ::new (static_cast<void *>(addressof(VariantImpl::get<I>(dst)))) T(move(VariantImpl::get<I>(src)));
            }

            template<size_type... Is>
            static constexpr auto move_ctor_table(index_sequence<Is...>)
            {
                using Fn = void (*)(StorageT &, StorageT &);
                return std::array<Fn, sizeof...(Is)>{&Base::move_ctor_one<Is>...};
            }

            void move_construct_from(Base &other) noexcept(Traits_::nothrow_move_ctor)
            {
                static constexpr auto table = move_ctor_table(make_index_sequence<sizeof...(Types)>());
                table[other._index](_storage, other._storage);
                _index = other._index;
            }

            // ---- copy-assign: same index -> assign in place, else destroy+construct ----
            template<size_type I>
            static void copy_assign_one(Base &self, const Base &other)
            {
                using T = AltT<I>;
                if (self._index == I)
                {
                    VariantImpl::get<I>(self._storage) = VariantImpl::get<I>(other._storage);
                } else
                {
                    self.destroy();
                    ::new (static_cast<void *>(addressof(VariantImpl::get<I>(self._storage))))
                            T(VariantImpl::get<I>(other._storage));
                    self._index = static_cast<IndexType>(I);
                }
            }

            template<size_type... Is>
            static constexpr auto copy_assign_table(index_sequence<Is...>)
            {
                using Fn = void (*)(Base &, const Base &);
                return std::array<Fn, sizeof...(Is)>{&Base::copy_assign_one<Is>...};
            }

            void copy_assign_from(const Base &other)
            {
                static constexpr auto table = copy_assign_table(make_index_sequence<sizeof...(Types)>());
                table[other._index](*this, other);
            }

            // ---- move-assign ----
            template<size_type I>
            static void move_assign_one(Base &self, Base &other)
            {
                using T = AltT<I>;
                if (self._index == I)
                {
                    VariantImpl::get<I>(self._storage) = move(VariantImpl::get<I>(other._storage));
                } else
                {
                    self.destroy();
                    ::new (static_cast<void *>(addressof(VariantImpl::get<I>(self._storage))))
                            T(move(VariantImpl::get<I>(other._storage)));
                    self._index = static_cast<IndexType>(I);
                }
            }

            template<size_type... Is>
            static constexpr auto move_assign_table(index_sequence<Is...>)
            {
                using Fn = void (*)(Base &, Base &);
                return std::array<Fn, sizeof...(Is)>{&Base::move_assign_one<Is>...};
            }

            void move_assign_from(Base &other) noexcept(Traits_::nothrow_move_ctor && Traits_::trivial_move_assign)
            {
                static constexpr auto table = move_assign_table(make_index_sequence<sizeof...(Types)>());
                table[other._index](*this, other);
            }

        public:
            ~Base() { destroy(); }

            Base(const Base &other) : _storage(), _index(0) { copy_construct_from(other); }

            Base(Base &&other) noexcept(Traits_::nothrow_move_ctor) : _storage(), _index(0)
            {
                move_construct_from(other);
            }

            Base &operator=(const Base &other)
            {
                if (this != &other)
                    copy_assign_from(other);
                return *this;
            }

            Base &operator=(Base &&other) noexcept(Traits_::nothrow_move_ctor && Traits_::trivial_move_assign)
            {
                if (this != &other)
                    move_assign_from(other);
                return *this;
            }
        };

        // ---------------- trivial base: let the compiler do everything ----------------
        template<typename... Types>
        class Base<true, Types...>
        {
        protected:
            using StorageT  = Storage<Types...>;
            using IndexType = index_type_for<sizeof...(Types)>;
            using Traits_   = Traits<Types...>;

            template<size_type I>
            using AltT = typename NthType<I, Types...>::type;

            StorageT _storage;
            IndexType _index;

            constexpr Base() noexcept(is_nothrow_default_constructible_v<AltT<0>>) :
                _storage(in_place_index<0>), _index(0)
            {
            }

            template<size_type I, typename... Args>
            constexpr explicit Base(in_place_index_t<I>, Args &&...args) :
                _storage(in_place_index<I>, forward<Args>(args)...), _index(static_cast<IndexType>(I))
            {
            }

            void destroy() noexcept {} // trivially destructible alternatives: nothing to do

        public:
            ~Base()                       = default;
            Base(const Base &)            = default;
            Base(Base &&)                 = default;
            Base &operator=(const Base &) = default;
            Base &operator=(Base &&)      = default;
        };
    } // namespace Detail::VariantImpl

    template<typename... Types>
    class variant : private Detail::VariantImpl::Base<Detail::VariantImpl::Traits<Types...>::all_trivial, Types...>
    {
        using Base_     = Detail::VariantImpl::Base<Detail::VariantImpl::Traits<Types...>::all_trivial, Types...>;
        using Traits_   = Detail::VariantImpl::Traits<Types...>;
        using StorageT  = typename Base_::StorageT;
        using IndexType = typename Base_::IndexType;

        static_assert(sizeof...(Types) > 0, "variant must have at least one alternative");
        static_assert((!is_reference_v<Types> && ...), "variant alternatives may not be references");
        static_assert((!is_array_v<Types> && ...), "variant alternatives may not be array types");

        template<size_type I>
        using AltT = typename NthType<I, Types...>::type;

        template<typename T>
        static constexpr size_type count_matches = (static_cast<size_type>(is_same_v<T, Types>) + ...);

        template<typename T>
        static constexpr bool exactly_one = count_matches<T> == 1;

        template<typename T>
        static constexpr size_type index_of_type()
        {
            static_assert(exactly_one<T>, "T must appear exactly once among the variant's alternatives");
            size_type i     = 0;
            size_type found = variant_npos;
            ((is_same_v<T, Types> ? (found = i, void()) : void(), ++i), ...);
            return found;
        }

    public:
        constexpr variant() = default;

        variant(const variant &)            = default;
        variant(variant &&)                 = default;
        variant &operator=(const variant &) = default;
        variant &operator=(variant &&)      = default;
        ~variant()                          = default;

        template<size_type I, typename... Args>
        constexpr explicit variant(in_place_index_t<I>, Args &&...args) :
            Base_(in_place_index<I>, forward<Args>(args)...)
        {
            static_assert(I < sizeof...(Types), "alternative index out of range");
        }

        template<typename T, typename... Args>
        constexpr explicit variant(in_place_type_t<T>, Args &&...args) :
            variant(in_place_index<index_of_type<T>()>, forward<Args>(args)...)
        {
        }

        template<typename T, typename = enable_if_t<!is_same_v<decay_t<T>, variant> && exactly_one<decay_t<T>>>>
        constexpr variant(T &&value) : variant(in_place_index<index_of_type<decay_t<T>>()>, forward<T>(value))
        {
        }

        constexpr size_type index() const noexcept { return this->_index; }

        template<typename T>
        constexpr bool holds_alternative() const noexcept
        {
            return this->_index == index_of_type<T>();
        }

        // ---- get<I>: unchecked in release, assert-guarded in debug ----
        template<size_type I>
        constexpr AltT<I> &get() & noexcept
        {
            assert(this->_index == I && "SFTL::variant::get<I>() -- wrong alternative active");
            return Detail::VariantImpl::get<I>(this->_storage);
        }

        template<size_type I>
        constexpr const AltT<I> &get() const & noexcept
        {
            assert(this->_index == I && "SFTL::variant::get<I>() -- wrong alternative active");
            return Detail::VariantImpl::get<I>(this->_storage);
        }

        template<size_type I>
        constexpr AltT<I> &&get() && noexcept
        {
            assert(this->_index == I && "SFTL::variant::get<I>() -- wrong alternative active");
            return move(Detail::VariantImpl::get<I>(this->_storage));
        }

        template<typename T>
        constexpr T &get() & noexcept
        {
            return get<index_of_type<T>()>();
        }

        template<typename T>
        constexpr const T &get() const & noexcept
        {
            return get<index_of_type<T>()>();
        }

        // ---- get_if<I>: runtime-checked pointer path ----
        template<size_type I>
        constexpr AltT<I> *get_if() noexcept
        {
            return this->_index == I ? addressof(Detail::VariantImpl::get<I>(this->_storage)) : nullptr;
        }

        template<size_type I>
        constexpr const AltT<I> *get_if() const noexcept
        {
            return this->_index == I ? addressof(Detail::VariantImpl::get<I>(this->_storage)) : nullptr;
        }

        template<typename T>
        constexpr T *get_if() noexcept
        {
            return get_if<index_of_type<T>()>();
        }

        template<typename T>
        constexpr const T *get_if() const noexcept
        {
            return get_if<index_of_type<T>()>();
        }

        // ---- emplace<I>: destroy current, construct new in place ----
        template<size_type I, typename... Args>
        AltT<I> &emplace(Args &&...args)
        {
            static_assert(I < sizeof...(Types), "alternative index out of range");
            this->destroy();
            using T = AltT<I>;
            ::new (static_cast<void *>(addressof(Detail::VariantImpl::get<I>(this->_storage))))
                    T(forward<Args>(args)...);
            this->_index = static_cast<IndexType>(I);
            return Detail::VariantImpl::get<I>(this->_storage);
        }

        template<typename T, typename... Args>
        T &emplace(Args &&...args)
        {
            return emplace<index_of_type<T>()>(forward<Args>(args)...);
        }

        void swap(variant &other) noexcept(Traits_::nothrow_swappable)
        {
            if (this == &other)
                return;
            variant tmp(move(other));
            other = move(*this);
            *this = move(tmp);
        }
    };

    namespace Detail::VariantImpl
    {
        template<typename Ret, typename Visitor, typename VarRef, size_type I>
        static Ret visit_trampoline(Visitor &&vis, VarRef v)
        {
            return static_cast<Ret>(forward<Visitor>(vis)(forward<VarRef>(v).template get<I>()));
        }
    } // namespace Detail::VariantImpl

    template<typename Visitor, typename... Types>
    decltype(auto) visit(Visitor &&vis, variant<Types...> &v)
    {
        using Ret                   = decltype(forward<Visitor>(vis)(v.template get<0>()));
        using Fn                    = Ret (*)(Visitor &&, variant<Types...> &);
        static constexpr auto table = []<size_type... Is>(index_sequence<Is...>)
        {
            return std::array<Fn, sizeof...(Types)>{
                    &Detail::VariantImpl::visit_trampoline<Ret, Visitor, variant<Types...> &, Is>...};
        }(make_index_sequence<sizeof...(Types)>());
        return table[v.index()](forward<Visitor>(vis), v);
    }

    template<typename Visitor, typename... Types>
    decltype(auto) visit(Visitor &&vis, const variant<Types...> &v)
    {
        using Ret                   = decltype(forward<Visitor>(vis)(v.template get<0>()));
        using Fn                    = Ret (*)(Visitor &&, const variant<Types...> &);
        static constexpr auto table = []<size_type... Is>(index_sequence<Is...>)
        {
            return std::array<Fn, sizeof...(Types)>{
                    &Detail::VariantImpl::visit_trampoline<Ret, Visitor, const variant<Types...> &, Is>...};
        }(make_index_sequence<sizeof...(Types)>());
        return table[v.index()](forward<Visitor>(vis), v);
    }

    template<typename Visitor, typename... Types>
    decltype(auto) visit(Visitor &&vis, variant<Types...> &&v)
    {
        using Ret                   = decltype(forward<Visitor>(vis)(move(v).template get<0>()));
        using Fn                    = Ret (*)(Visitor &&, variant<Types...> &&);
        static constexpr auto table = []<size_type... Is>(index_sequence<Is...>)
        {
            return std::array<Fn, sizeof...(Types)>{
                    &Detail::VariantImpl::visit_trampoline<Ret, Visitor, variant<Types...> &&, Is>...};
        }(make_index_sequence<sizeof...(Types)>());
        return table[v.index()](forward<Visitor>(vis), move(v));
    }

    template<typename T, typename... Types>
    constexpr bool holds_alternative(const variant<Types...> &v) noexcept
    {
        return v.template holds_alternative<T>();
    }

    template<size_type I, typename... Types>
    constexpr variant_alternative_t<I, variant<Types...>> &get(variant<Types...> &v) noexcept
    {
        return v.template get<I>();
    }

    template<size_type I, typename... Types>
    constexpr const variant_alternative_t<I, variant<Types...>> &get(const variant<Types...> &v) noexcept
    {
        return v.template get<I>();
    }

    template<typename T, typename... Types>
    constexpr T &get(variant<Types...> &v) noexcept
    {
        return v.template get<T>();
    }

    template<typename T, typename... Types>
    constexpr const T &get(const variant<Types...> &v) noexcept
    {
        return v.template get<T>();
    }

    template<size_type I, typename... Types>
    constexpr variant_alternative_t<I, variant<Types...>> *get_if(variant<Types...> *v) noexcept
    {
        return v ? v->template get_if<I>() : nullptr;
    }

    template<typename T, typename... Types>
    constexpr T *get_if(variant<Types...> *v) noexcept
    {
        return v ? v->template get_if<T>() : nullptr;
    }

    template<typename... Types>
    void swap(variant<Types...> &lhs, variant<Types...> &rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    template<typename... Types>
    bool operator==(const variant<Types...> &lhs, const variant<Types...> &rhs)
    {
        if (lhs.index() != rhs.index())
            return false;
        return visit(
                [&]<typename T0>(const T0 &l) -> bool
                {
                    using T = decay_t<T0>;
                    return l == rhs.template get<T>();
                },
                lhs);
    }

    template<typename... Types>
    bool operator!=(const variant<Types...> &lhs, const variant<Types...> &rhs)
    {
        return !(lhs == rhs);
    }

} // namespace SFTL
