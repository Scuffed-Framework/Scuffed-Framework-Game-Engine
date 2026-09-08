#pragma once
#include "../ReferenceWrapper.hpp"
#include "../TypeTraits.hpp"

namespace SFTL
{
    // Forward declarations for tuple size/element traits
    template<typename... _Elements>
    struct tuple;

    template<typename Type>
    struct tuple_size;

    template<size_type _Ind, typename Type>
    struct tuple_element;

    namespace Detail
    {
        // Recursive tuple implementation storage
        template<size_type _Idx, typename Type>
        struct _Tuple_node
        {
            Type Lhead;

            constexpr _Tuple_node() : Lhead() {}

            template<typename _Ugp>
            constexpr explicit _Tuple_node(_Ugp &&__val) : Lhead(forward<_Ugp>(__val))
            {
            }
        };

        template<size_type _Idx, typename... _Elements>
        struct _Tuple_impl;

        // Base case: empty tuple
        template<size_type _Idx>
        struct _Tuple_impl<_Idx>
        {
            constexpr _Tuple_impl() = default;
        };

        // Recursive case
        template<size_type _Idx, typename _Head, typename... _Tail>
        struct _Tuple_impl<_Idx, _Head, _Tail...> : private _Tuple_node<_Idx, _Head>,
                                                    private _Tuple_impl<_Idx + 1, _Tail...>
        {
            using _Base_head = _Tuple_node<_Idx, _Head>;
            using _Base_tail = _Tuple_impl<_Idx + 1, _Tail...>;

            constexpr _Tuple_impl() : _Base_head(), _Base_tail() {}

            template<typename _UHead, typename... _UTail>
            constexpr explicit _Tuple_impl(_UHead &&__head, _UTail &&...__tail) :
                _Base_head(forward<_UHead>(__head)), _Base_tail(forward<_UTail>(__tail)...)
            {
            }

            template<size_type _Nm, typename... _Elms>
            friend constexpr auto &_Tuple_get(_Tuple_impl<_Nm, _Elms...> &__t) noexcept;
        };

        // Helper getters for implementation nodes
        template<size_type _Nm, typename _Head, typename... _Tail>
        constexpr auto &_Tuple_get(_Tuple_impl<_Nm, _Head, _Tail...> &__in) noexcept
        {
            if constexpr (_Nm == 0)
                return static_cast<_Tuple_node<_Nm, _Head> &>(__in).Lhead;
            else
                return _Tuple_get<_Nm - 1>(static_cast<_Tuple_impl<_Nm + 1, _Tail...> &>(__in));
        }

        template<size_type _Nm, typename _Head, typename... _Tail>
        constexpr const auto &_Tuple_get(const _Tuple_impl<_Nm, _Head, _Tail...> &__in) noexcept
        {
            if constexpr (_Nm == 0)
                return static_cast<const _Tuple_node<_Nm, _Head> &>(__in).Lhead;
            else
                return _Tuple_get<_Nm - 1>(static_cast<const _Tuple_impl<_Nm + 1, _Tail...> &>(__in));
        }
    } // namespace Detail

    // Primary tuple definition
    template<typename... _Elements>
    struct tuple : private Detail::_Tuple_impl<0, _Elements...>
    {
        using _Inherited = Detail::_Tuple_impl<0, _Elements...>;

        constexpr tuple() : _Inherited() {}

        template<typename... _UElements>
        constexpr explicit tuple(_UElements &&...__args) : _Inherited(forward<_UElements>(__args)...)
        {
        }

        template<size_type _Nm, typename... _Elms>
        friend constexpr auto &Detail::_Tuple_get(Detail::_Tuple_impl<_Nm, _Elms...> &__t) noexcept;

        template<size_type _Nm, typename... _Elms>
        friend constexpr const auto &Detail::_Tuple_get(const Detail::_Tuple_impl<_Nm, _Elms...> &__t) noexcept;
    };

    // Deduction guide
    template<typename... _Elements>
    tuple(_Elements...) -> tuple<_Elements...>;

    // --- Tuple Traits ---

    template<typename... _Elements>
    struct tuple_size<tuple<_Elements...>> : integral_constant<size_type, sizeof...(_Elements)>
    {
    };

    template<typename... _Elements>
    struct tuple_size<const tuple<_Elements...>> : integral_constant<size_type, sizeof...(_Elements)>
    {
    };

    template<size_type _I, typename _Head, typename... _Tail>
    struct tuple_element<_I, tuple<_Head, _Tail...>> : tuple_element<_I - 1, tuple<_Tail...>>
    {
    };

    template<typename _Head, typename... _Tail>
    struct tuple_element<0, tuple<_Head, _Tail...>>
    {
        using type = _Head;
    };

    template<size_type _I, typename... _Elements>
    using tuple_element_t = tuple_element<_I, tuple<_Elements...>>::type;

    template<size_type _Ip, typename... _Elements>
    [[nodiscard]] constexpr auto &get(tuple<_Elements...> &__t) noexcept
    {
        return Detail::_Tuple_get<_Ip>(static_cast<Detail::_Tuple_impl<0, _Elements...> &>(__t));
    }

    template<size_type _Ip, typename... _Elements>
    [[nodiscard]] constexpr const auto &get(const tuple<_Elements...> &__t) noexcept
    {
        return Detail::_Tuple_get<_Ip>(static_cast<const Detail::_Tuple_impl<0, _Elements...> &>(__t));
    }

    template<typename... _Elements>
    [[nodiscard]] constexpr auto make_tuple(_Elements &&...__args)
    {
        return tuple<unwrap_ref_decay_t<_Elements>...>(forward<_Elements>(__args)...);
    }

} // namespace SFTL
