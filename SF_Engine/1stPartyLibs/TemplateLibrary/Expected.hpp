#pragma once
#include "Containers/InitializerList.hpp"
#include "Invoke.hpp"
#include "Operations.hpp"
#include "TypeTraits.hpp"

namespace SFTL
{
    template<typename Type, typename Error>
    class expected;

    template<typename Error>
    class unexpected;

    struct unexpect_t
    {
        explicit unexpect_t() = default;
    };

    inline constexpr unexpect_t unexpect{};

    namespace ExpectedImpl
    {
        template<typename Type>
        constexpr bool is_expected_impl = false;
        template<typename Type, typename Error>
        constexpr bool is_expected_impl<expected<Type, Error>> = true;

        template<typename Type>
        constexpr bool is_unexpected_impl = false;
        template<typename Type>
        constexpr bool is_unexpected_impl<unexpected<Type>> = true;

        template<typename Function, typename Type>
        using result = remove_cvref_t<invoke_result_t<Function &&, Type &&>>;
        template<typename Function, typename Type>
        using result_from = remove_cv_t<invoke_result_t<Function &&, Type &&>>;
        template<typename Function>
        using result0 = remove_cvref_t<invoke_result_t<Function &&>>;
        template<typename Function>
        using result0_from = remove_cv_t<invoke_result_t<Function &&>>;

        template<typename Error>
        concept can_be_inexcpected =
                is_object_v<Error> && (!is_array_v<Error>) && (!ExpectedImpl::is_unexpected_impl<Error>) &&
                (!is_const_v<Error>) && (!is_volatile_v<Error>);

        struct in_place_inv
        {
        };

        struct unexpected_val
        {
        };
    } // namespace ExpectedImpl

    template<typename Error>
    class unexpected
    {
        static_assert(ExpectedImpl::can_be_inexcpected<Error>);

    public:
        constexpr unexpected(const unexpected &) = default;
        constexpr unexpected(unexpected &&)      = default;

        template<typename error = Error>
            requires(!is_same_v<remove_cvref_t<error>, unexpected>) &&
                    (!is_same_v<remove_cvref_t<error>, in_place_t>) && is_constructible_v<Error, error>
        constexpr explicit unexpected(error &&e) noexcept(is_nothrow_constructible_v<Error, error>) :
            ERR_Unexpected(forward<error>(e))
        {
        }

        template<typename... _Args>
            requires is_constructible_v<Error, _Args...>
        constexpr explicit unexpected(in_place_t,
                                      _Args &&...args) noexcept(is_nothrow_constructible_v<Error, _Args...>) :
            ERR_Unexpected(forward<_Args>(args)...)
        {
        }

        template<typename Up, typename... _Args>
            requires is_constructible_v<Error, initializer_list<Up> &, _Args...>
        constexpr explicit unexpected(in_place_t, initializer_list<Up> list, _Args &&...args) noexcept(
                is_nothrow_constructible_v<Error, initializer_list<Up> &, _Args...>) :
            ERR_Unexpected(list, forward<_Args>(args)...)
        {
        }

        constexpr unexpected &operator=(const unexpected &) = default;
        constexpr unexpected &operator=(unexpected &&)      = default;


        [[nodiscard]]
        constexpr const Error &error() const & noexcept
        {
            return ERR_Unexpected;
        }

        [[nodiscard]]
        constexpr Error &error() & noexcept
        {
            return ERR_Unexpected;
        }

        [[nodiscard]]
        constexpr const Error &&error() const && noexcept
        {
            return move(ERR_Unexpected);
        }

        [[nodiscard]]
        constexpr Error &&error() && noexcept
        {
            return move(ERR_Unexpected);
        }

        constexpr void swap(unexpected &other) noexcept(is_nothrow_swappable_v<Error>)
            requires is_swappable_v<Error>
        {
            using swap;
            swap(ERR_Unexpected, other.ERR_Unexpected);
        }

        template<typename error>
        [[nodiscard]]
        friend constexpr bool operator==(const unexpected &value, const unexpected<error> &y)
        {
            return value.ERR_Unexpected == y.error();
        }

        friend constexpr void swap(unexpected &value, unexpected &y) noexcept(noexcept(value.swap(y)))
            requires is_swappable_v<Error>
        {
            value.swap(y);
        }

    private:
        Error ERR_Unexpected;
    };

    template<typename Error>
    unexpected(Error) -> unexpected<Error>;

    namespace ExpectedImpl
    {
        template<typename Type>
        struct Guard
        {
            constexpr explicit Guard(Type &value) : Guarded(::SFTL::addressof(value)), _tmp(move(value))
            {
                destroy_at(Guarded);
            }

            constexpr ~Guard()
            {
                if (Guarded) [[unlikely]]
                    construct_at(Guarded, move(_tmp));
            }

            Guard(const Guard &)            = delete;
            Guard &operator=(const Guard &) = delete;

            constexpr Type &&release() noexcept
            {
                Guarded = nullptr;
                return move(_tmp);
            }

        private:
            Type *Guarded;
            Type _tmp;
        };

        template<typename Type, typename Up, typename old>
        constexpr void reinit(Type *NewValue, Up *OldValue, old &&arg) noexcept(is_nothrow_constructible_v<Type, old>)
        {
            if constexpr (is_nothrow_constructible_v<Type, old>)
            {
                destroy_at(OldValue);
                construct_at(NewValue, forward<old>(arg));
            } else if constexpr (is_nothrow_move_constructible_v<Type>)
            {
                Type tmp(forward<old>(arg));
                destroy_at(OldValue);
                construct_at(NewValue, move(tmp));
            } else
            {
                Guard<Up> guard(*OldValue);
                construct_at(NewValue, forward<old>(arg));
                guard.release();
            }
        }

        template<typename Type, typename Up>
        concept not_constructing_bool_from_expected =
                !is_same_v<remove_cv_t<Type>, bool> || !is_expected_impl<remove_cvref_t<Up>>;

        template<typename Type, typename Up = remove_cvref_t<Type>>
        concept trivially_replaceable = is_trivially_constructible_v<Up, Type> &&
                                        is_trivially_assignable_v<Up &, Type> && is_trivially_destructible_v<Up>;

        template<typename Type, typename Up = remove_cvref_t<Type>>
        concept _unexpectedsable_for_assign = is_constructible_v<Up, Type> && is_assignable_v<Up &, Type>;

        template<typename Type>
        concept _unexpectedsable_for_trivial_assign = trivially_replaceable<Type> && _unexpectedsable_for_assign<Type>;

        template<typename Type, typename Error>
        concept can_reassign_type = is_nothrow_move_constructible_v<Type> || is_nothrow_move_constructible_v<Error>;
    } // namespace ExpectedImpl

    template<typename Type, typename Error>
    class [[nodiscard]] expected
    {
        static_assert(!is_reference_v<Type>);
        static_assert(!is_function_v<Type>);
        static_assert(!is_same_v<remove_cv_t<Type>, in_place_t>);
        static_assert(!is_same_v<remove_cv_t<Type>, unexpect_t>);
        static_assert(!ExpectedImpl::is_unexpected_impl<remove_cv_t<Type>>);
        static_assert(ExpectedImpl::can_be_inexcpected<Error>);

        template<typename Up, typename Gr, typename _Unex = unexpected<Error>, typename = remove_cv_t<Type>>
        static constexpr bool from_expected =
                or_v<is_constructible<Type, expected<Up, Gr> &>, is_constructible<Type, expected<Up, Gr>>,
                     is_constructible<Type, const expected<Up, Gr> &>, is_constructible<Type, const expected<Up, Gr>>,
                     is_convertible<expected<Up, Gr> &, Type>, is_convertible<expected<Up, Gr>, Type>,
                     is_convertible<const expected<Up, Gr> &, Type>, is_convertible<const expected<Up, Gr>, Type>,
                     is_constructible<_Unex, expected<Up, Gr> &>, is_constructible<_Unex, expected<Up, Gr>>,
                     is_constructible<_Unex, const expected<Up, Gr> &>,
                     is_constructible<_Unex, const expected<Up, Gr>>>;

        template<typename Up, typename Gr, typename _Unex>
        static constexpr bool from_expected<Up, Gr, _Unex, bool> =
                or_v<is_constructible<_Unex, expected<Up, Gr> &>, is_constructible<_Unex, expected<Up, Gr>>,
                     is_constructible<_Unex, const expected<Up, Gr> &>,
                     is_constructible<_Unex, const expected<Up, Gr>>>;

        template<typename Up, typename Gr>
        constexpr static bool explicit_conversion = or_v<Not<is_convertible<Up, Type>>, Not<is_convertible<Gr, Error>>>;

        template<typename Up>
        static constexpr bool same_value = is_same_v<typename Up::value_type, Type>;

        template<typename Up>
        static constexpr bool same_error = is_same_v<typename Up::error_type, Error>;

    public:
        using value_type      = Type;
        using error_type      = Error;
        using unexpected_type = unexpected<Error>;

        template<typename Up>
        using rebind = expected<Up, error_type>;

        constexpr expected() noexcept(is_nothrow_default_constructible_v<Type>)
            requires is_default_constructible_v<Type>
            : Value(), HasValue(true)
        {
        }

        expected(const expected &) = default;

        constexpr expected(const expected &value) noexcept(
                and_v<is_nothrow_copy_constructible<Type>, is_nothrow_copy_constructible<Error>>)
            requires is_copy_constructible_v<Type> && is_copy_constructible_v<Error> &&
                     (!is_trivially_copy_constructible_v<Type> || !is_trivially_copy_constructible_v<Error>)
            : HasValue(value.HasValue)
        {
            if (HasValue)
                construct_at(::SFTL::addressof(Value), value.Value);
            else
                construct_at(::SFTL::addressof(ERR_Unexpected), value.ERR_Unexpected);
        }

        expected(expected &&) = default;

        constexpr expected(expected &&value) noexcept(
                and_v<is_nothrow_move_constructible<Type>, is_nothrow_move_constructible<Error>>)
            requires is_move_constructible_v<Type> && is_move_constructible_v<Error> &&
                     (!is_trivially_move_constructible_v<Type> || !is_trivially_move_constructible_v<Error>)
            : HasValue(value.HasValue)
        {
            if (HasValue)
                construct_at(::SFTL::addressof(Value), move(value).Value);
            else
                construct_at(::SFTL::addressof(ERR_Unexpected), move(value).ERR_Unexpected);
        }

        template<typename Up, typename Gr>
            requires is_constructible_v<Type, const Up &> && is_constructible_v<Error, const Gr &> &&
                     (!from_expected<Up, Gr>)
        constexpr explicit(explicit_conversion<const Up &, const Gr &>)
                expected(const expected<Up, Gr> &value) noexcept(and_v<is_nothrow_constructible<Type, const Up &>,
                                                                       is_nothrow_constructible<Error, const Gr &>>) :
            HasValue(value.HasValue)
        {
            if (HasValue)
                construct_at(::SFTL::addressof(Value), value.Value);
            else
                construct_at(::SFTL::addressof(ERR_Unexpected), value.ERR_Unexpected);
        }

        template<typename Up, typename Gr>
            requires is_constructible_v<Type, Up> && is_constructible_v<Error, Gr> && (!from_expected<Up, Gr>)
        constexpr explicit(explicit_conversion<Up, Gr>) expected(expected<Up, Gr> &&value) noexcept(
                and_v<is_nothrow_constructible<Type, Up>, is_nothrow_constructible<Error, Gr>>) :
            HasValue(value.HasValue)
        {
            if (HasValue)
                construct_at(::SFTL::addressof(Value), move(value).Value);
            else
                construct_at(::SFTL::addressof(ERR_Unexpected), move(value).ERR_Unexpected);
        }

        template<typename Up = remove_cv_t<Type>>
            requires(!is_same_v<remove_cvref_t<Up>, expected>) && (!is_same_v<remove_cvref_t<Up>, in_place_t>) &&
                            (!is_same_v<remove_cvref_t<Up>, unexpect_t>) && is_constructible_v<Type, Up> &&
                            (!ExpectedImpl::is_unexpected_impl<remove_cvref_t<Up>>) &&
                            ExpectedImpl::not_constructing_bool_from_expected<Type, Up>
        constexpr explicit(!is_convertible_v<Up, Type>)
                expected(Up &&val) noexcept(is_nothrow_constructible_v<Type, Up>) :
            Value(forward<Up>(val)), HasValue(true)
        {
        }

        template<typename Gr = Error>
            requires is_constructible_v<Error, const Gr &>
        constexpr explicit(!is_convertible_v<const Gr &, Error>)
                expected(const unexpected<Gr> &_unexpected) noexcept(is_nothrow_constructible_v<Error, const Gr &>) :
            ERR_Unexpected(_unexpected.error()), HasValue(false)
        {
        }

        template<typename Gr = Error>
            requires is_constructible_v<Error, Gr>
        constexpr explicit(!is_convertible_v<Gr, Error>)
                expected(unexpected<Gr> &&_unexpected) noexcept(is_nothrow_constructible_v<Error, Gr>) :
            ERR_Unexpected(move(_unexpected).error()), HasValue(false)
        {
        }

        template<typename... _Args>
            requires is_constructible_v<Type, _Args...>
        constexpr explicit expected(in_place_t, _Args &&...args) noexcept(is_nothrow_constructible_v<Type, _Args...>) :
            Value(forward<_Args>(args)...), HasValue(true)
        {
        }

        template<typename Up, typename... _Args>
            requires is_constructible_v<Type, initializer_list<Up> &, _Args...>
        constexpr explicit expected(in_place_t, initializer_list<Up> list, _Args &&...args) noexcept(
                is_nothrow_constructible_v<Type, initializer_list<Up> &, _Args...>) :
            Value(list, forward<_Args>(args)...), HasValue(true)
        {
        }

        template<typename... _Args>
            requires is_constructible_v<Error, _Args...>
        constexpr explicit expected(unexpect_t, _Args &&...args) noexcept(is_nothrow_constructible_v<Error, _Args...>) :
            ERR_Unexpected(forward<_Args>(args)...), HasValue(false)
        {
        }

        template<typename Up, typename... _Args>
            requires is_constructible_v<Error, initializer_list<Up> &, _Args...>
        constexpr explicit expected(unexpect_t, initializer_list<Up> list, _Args &&...args) noexcept(
                is_nothrow_constructible_v<Error, initializer_list<Up> &, _Args...>) :
            ERR_Unexpected(list, forward<_Args>(args)...), HasValue(false)
        {
        }

        constexpr ~expected() = default;

        constexpr ~expected()
            requires(!is_trivially_destructible_v<Type>) || (!is_trivially_destructible_v<Error>)
        {
            if (HasValue)
                destroy_at(::SFTL::addressof(Value));
            else
                destroy_at(::SFTL::addressof(ERR_Unexpected));
        }
        expected &operator=(const expected &) = delete;
        expected &operator=(const expected &) noexcept(
                and_v<is_nothrow_copy_constructible<Type>, is_nothrow_copy_constructible<Error>,
                      is_nothrow_copy_assignable<Type>, is_nothrow_copy_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_trivial_assign<const Type &> &&
                             ExpectedImpl::_unexpectedsable_for_trivial_assign<const Error &> &&
                             ExpectedImpl::can_reassign_type<Type, Error>
        = default;

        constexpr expected &operator=(const expected &value) noexcept(
                and_v<is_nothrow_copy_constructible<Type>, is_nothrow_copy_constructible<Error>,
                      is_nothrow_copy_assignable<Type>, is_nothrow_copy_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_assign<const Type &> &&
                     ExpectedImpl::_unexpectedsable_for_assign<const Error &> &&
                     ExpectedImpl::can_reassign_type<Type, Error>
        {
            if (value.HasValue)
                this->_assign_val(value.Value);
            else
                this->assign_unexpected(value.ERR_Unexpected);
            return *this;
        }

        expected &
        operator=(expected &&) noexcept(and_v<is_nothrow_move_constructible<Type>, is_nothrow_move_constructible<Error>,
                                              is_nothrow_move_assignable<Type>, is_nothrow_move_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_trivial_assign<Type &&> &&
                             ExpectedImpl::_unexpectedsable_for_trivial_assign<Error &&> &&
                             ExpectedImpl::can_reassign_type<Type, Error>
        = default;

        // Non-trivial move assignment
        constexpr expected &operator=(expected &&value) noexcept(
                and_v<is_nothrow_move_constructible<Type>, is_nothrow_move_constructible<Error>,
                      is_nothrow_move_assignable<Type>, is_nothrow_move_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_assign<Type &&> &&
                     ExpectedImpl::_unexpectedsable_for_assign<Error &&> && ExpectedImpl::can_reassign_type<Type, Error>
        {
            if (value.HasValue)
                _assign_val(move(value.Value));
            else
                assign_unexpected(move(value.ERR_Unexpected));
            return *this;
        }

        template<typename Up = remove_cv_t<Type>>
            requires(!is_same_v<expected, remove_cvref_t<Up>>) &&
                    (!ExpectedImpl::is_unexpected_impl<remove_cvref_t<Up>>) && is_constructible_v<Type, Up> &&
                    is_assignable_v<Type &, Up> &&
                    (is_nothrow_constructible_v<Type, Up> || is_nothrow_move_constructible_v<Type> ||
                     is_nothrow_move_constructible_v<Error>)
        constexpr expected &operator=(Up &&val)
        {
            _assign_val(forward<Up>(val));
            return *this;
        }

        template<typename Gr>
            requires is_constructible_v<Error, const Gr &> && is_assignable_v<Error &, const Gr &> &&
                     (is_nothrow_constructible_v<Error, const Gr &> || is_nothrow_move_constructible_v<Type> ||
                      is_nothrow_move_constructible_v<Error>)
        constexpr expected &operator=(const unexpected<Gr> &e)
        {
            assign_unexpected(e.error());
            return *this;
        }

        template<typename Gr>
            requires is_constructible_v<Error, Gr> && is_assignable_v<Error &, Gr> &&
                     (is_nothrow_constructible_v<Error, Gr> || is_nothrow_move_constructible_v<Type> ||
                      is_nothrow_move_constructible_v<Error>)
        constexpr expected &operator=(unexpected<Gr> &&e)
        {
            assign_unexpected(move(e).error());
            return *this;
        }

        template<typename... _Args>
            requires is_nothrow_constructible_v<Type, _Args...>
        constexpr Type &emplace(_Args &&...args) noexcept
        {
            if (HasValue)
                destroy_at(::SFTL::addressof(Value));
            else
            {
                destroy_at(::SFTL::addressof(ERR_Unexpected));
                HasValue = true;
            }
            construct_at(::SFTL::addressof(Value), forward<_Args>(args)...);
            return Value;
        }

        template<typename Up, typename... _Args>
            requires is_nothrow_constructible_v<Type, initializer_list<Up> &, _Args...>
        constexpr Type &emplace(initializer_list<Up> list, _Args &&...args) noexcept
        {
            if (HasValue)
                destroy_at(::SFTL::addressof(Value));
            else
            {
                destroy_at(::SFTL::addressof(ERR_Unexpected));
                HasValue = true;
            }
            construct_at(::SFTL::addressof(Value), list, forward<_Args>(args)...);
            return Value;
        }

        constexpr void
        swap(expected &value) noexcept(and_v<is_nothrow_move_constructible<Type>, is_nothrow_move_constructible<Error>,
                                             is_nothrow_swappable<Type &>, is_nothrow_swappable<Error &>>)
            requires is_swappable_v<Type> && is_swappable_v<Error> && is_move_constructible_v<Type> &&
                     is_move_constructible_v<Error> &&
                     (is_nothrow_move_constructible_v<Type> || is_nothrow_move_constructible_v<Error>)
        {
            if (HasValue)
            {
                if (value.HasValue)
                {
                    ::SFTL::swap(Value, value.Value);
                } else
                    this->_swap_val_unex(value);
            } else
            {
                if (value.HasValue)
                    value._swap_val_unex(*this);
                else
                {
                    ;
                    ::SFTL::swap(ERR_Unexpected, value.ERR_Unexpected);
                }
            }
        }

        [[nodiscard]] constexpr const Type *operator->() const noexcept { return ::SFTL::addressof(Value); }
        [[nodiscard]] constexpr Type *operator->() noexcept { return ::SFTL::addressof(Value); }
        [[nodiscard]] constexpr const Type &operator*() const & noexcept { return Value; }
        [[nodiscard]] constexpr Type &operator*() & noexcept { return Value; }
        [[nodiscard]] constexpr const Type &&operator*() const && noexcept { return move(Value); }
        [[nodiscard]] constexpr Type &&operator*() && noexcept { return move(Value); }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return HasValue; }
        [[nodiscard]] constexpr bool has_value() const noexcept { return HasValue; }

        constexpr const Type &value() const &
        {
            static_assert(is_copy_constructible_v<Error>);
            if (HasValue) [[likely]]
                return Value;
            return nullptr;
        }

        constexpr Type &value() &
        {
            static_assert(is_copy_constructible_v<Error>);
            if (HasValue) [[likely]]
                return Value;
            const auto &_unexpected = ERR_Unexpected;
            return _unexpected;
        }

        constexpr const Type &&value() const &&
        {
            static_assert(is_copy_constructible_v<Error>);
            static_assert(is_constructible_v<Error, const Error &&>);
            if (HasValue) [[likely]]
                return move(Value);
            return nullptr;
        }

        constexpr Type &&value() &&
        {
            static_assert(is_copy_constructible_v<Error>);
            static_assert(is_constructible_v<Error, Error &&>);
            if (HasValue) [[likely]]
                return move(Value);
            return nullptr;
        }

        constexpr const Error &error() const & noexcept { return ERR_Unexpected; }
        constexpr Error &error() & noexcept { return ERR_Unexpected; }
        constexpr const Error &&error() const && noexcept { return move(ERR_Unexpected); }
        constexpr Error &&error() && noexcept { return move(ERR_Unexpected); }

        template<typename Up = remove_cv_t<Type>>
        constexpr remove_cv_t<Type> value_or(Up &&val) const & noexcept(
                and_v<is_nothrow_copy_constructible<Type>, is_nothrow_convertible<Up, Type>>)
        {
            using _Xp = remove_cv_t<Type>;
            static_assert(is_convertible_v<const Type &, _Xp>);
            static_assert(is_convertible_v<Up, Type>);

            if (HasValue)
                return Value;
            return forward<Up>(val);
        }

        template<typename Up = remove_cv_t<Type>>
        constexpr remove_cv_t<Type>
        value_or(Up &&val) && noexcept(and_v<is_nothrow_move_constructible<Type>, is_nothrow_convertible<Up, Type>>)
        {
            using _Xp = remove_cv_t<Type>;
            static_assert(is_convertible_v<Type, _Xp>);
            static_assert(is_convertible_v<Up, _Xp>);

            if (HasValue)
                return move(Value);
            return forward<Up>(val);
        }

        template<typename Gr = Error>
        constexpr Error error_or(Gr &&e) const &
        {
            static_assert(is_copy_constructible_v<Error>);
            static_assert(is_convertible_v<Gr, Error>);

            if (HasValue)
                return forward<Gr>(e);
            return ERR_Unexpected;
        }

        template<typename Gr = Error>
        constexpr Error error_or(Gr &&e) &&
        {
            static_assert(is_move_constructible_v<Error>);
            static_assert(is_convertible_v<Gr, Error>);

            if (HasValue)
                return forward<Gr>(e);
            return move(ERR_Unexpected);
        }

        // monadic operations

        template<typename Function>
            requires is_constructible_v<Error, Error &>
        constexpr auto and_then(Function &&f) &
        {
            using Up = ExpectedImpl::result<Function, Type &>;
            static_assert(ExpectedImpl::is_expected_impl<Up>, "the function passed to expected<T, E>::and_then "
                                                              "must return a expected");
            static_assert(is_same_v<typename Up::error_type, Error>, "the function passed to expected<T, E>::and_then "
                                                                     "must return a expected with the same error_type");

            if (has_value())
                return invoke(forward<Function>(f), Value);
            else
                return Up(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error &>
        constexpr auto and_then(Function &&f) const &
        {
            using Up = ExpectedImpl::result<Function, const Type &>;
            static_assert(ExpectedImpl::is_expected_impl<Up>, "the function passed to expected<T, E>::and_then "
                                                              "must return a expected");
            static_assert(is_same_v<typename Up::error_type, Error>, "the function passed to expected<T, E>::and_then "
                                                                     "must return a expected with the same error_type");

            if (has_value())
                return invoke(forward<Function>(f), Value);
            else
                return Up(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, Error>
        constexpr auto and_then(Function &&f) &&
        {
            using Up = ExpectedImpl::result<Function, Type &&>;
            static_assert(ExpectedImpl::is_expected_impl<Up>, "the function passed to expected<T, E>::and_then "
                                                              "must return a expected");
            static_assert(is_same_v<typename Up::error_type, Error>, "the function passed to expected<T, E>::and_then "
                                                                     "must return a expected with the same error_type");

            if (has_value())
                return invoke(forward<Function>(f), move(Value));
            else
                return Up(unexpect, move(ERR_Unexpected));
        }


        template<typename Function>
            requires is_constructible_v<Error, const Error>
        constexpr auto and_then(Function &&f) const &&
        {
            using Up = ExpectedImpl::result<Function, const Type &&>;
            static_assert(ExpectedImpl::is_expected_impl<Up>, "the function passed to expected<T, E>::and_then "
                                                              "must return a expected");
            static_assert(is_same_v<typename Up::error_type, Error>, "the function passed to expected<T, E>::and_then "
                                                                     "must return a expected with the same error_type");

            if (has_value())
                return invoke(forward<Function>(f), move(Value));
            else
                return Up(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Type, Type &>
        constexpr auto or_else(Function &&f) &
        {
            using Gr = ExpectedImpl::result<Function, Error &>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>, "the function passed to expected<T, E>::or_else "
                                                              "must return a expected");
            static_assert(is_same_v<typename Gr::value_type, Type>, "the function passed to expected<T, E>::or_else "
                                                                    "must return a expected with the same value_type");

            if (has_value())
                return Gr(in_place, Value);
            else
                return invoke(forward<Function>(f), ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Type, const Type &>
        constexpr auto or_else(Function &&f) const &
        {
            using Gr = ExpectedImpl::result<Function, const Error &>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>, "the function passed to expected<T, E>::or_else "
                                                              "must return a expected");
            static_assert(is_same_v<typename Gr::value_type, Type>, "the function passed to expected<T, E>::or_else "
                                                                    "must return a expected with the same value_type");

            if (has_value())
                return Gr(in_place, Value);
            else
                return invoke(forward<Function>(f), ERR_Unexpected);
        }


        template<typename Function>
            requires is_constructible_v<Type, Type>
        constexpr auto or_else(Function &&f) &&
        {
            using Gr = ExpectedImpl::result<Function, Error &&>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>, "the function passed to expected<T, E>::or_else "
                                                              "must return a expected");
            static_assert(is_same_v<typename Gr::value_type, Type>, "the function passed to expected<T, E>::or_else "
                                                                    "must return a expected with the same value_type");

            if (has_value())
                return Gr(in_place, move(Value));
            else
                return invoke(forward<Function>(f), move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Type, const Type>
        constexpr auto or_else(Function &&f) const &&
        {
            using Gr = ExpectedImpl::result<Function, const Error &&>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>, "the function passed to expected<T, E>::or_else "
                                                              "must return a expected");
            static_assert(is_same_v<typename Gr::value_type, Type>, "the function passed to expected<T, E>::or_else "
                                                                    "must return a expected with the same value_type");

            if (has_value())
                return Gr(in_place, move(Value));
            else
                return invoke(forward<Function>(f), move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Error, Error &>
        constexpr auto transform(Function &&f) &
        {
            using Up   = ExpectedImpl::result_from<Function, Type &>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, [&]() { return invoke(forward<Function>(f), Value); });
            else
                return _Res(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error &>
        constexpr auto transform(Function &&f) const &
        {
            using Up   = ExpectedImpl::result_from<Function, const Type &>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, [&]() { return invoke(forward<Function>(f), Value); });
            else
                return _Res(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, Error>
        constexpr auto transform(Function &&f) &&
        {
            using Up   = ExpectedImpl::result_from<Function, Type>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, [&]() { return invoke(forward<Function>(f), move(Value)); });
            else
                return _Res(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error>
        constexpr auto transform(Function &&f) const &&
        {
            using Up   = ExpectedImpl::result_from<Function, const Type>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, [&]() { return invoke(forward<Function>(f), move(Value)); });
            else
                return _Res(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Type, Type &>
        constexpr auto transform_error(Function &&f) &
        {
            using Gr   = ExpectedImpl::result_from<Function, Error &>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res(in_place, Value);
            return _Res([&]() { return invoke(forward<Function>(f), ERR_Unexpected); });
        }

        template<typename Function>
            requires is_constructible_v<Type, const Type &>
        constexpr auto transform_error(Function &&f) const &
        {
            using Gr   = ExpectedImpl::result_from<Function, const Error &>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res(in_place, Value);
            return _Res([&]() { return invoke(forward<Function>(f), ERR_Unexpected); });
        }

        template<typename Function>
            requires is_constructible_v<Type, Type>
        constexpr auto transform_error(Function &&f) &&
        {
            using Gr   = ExpectedImpl::result_from<Function, Error &&>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res(in_place, move(Value));
            return _Res([&]() { return invoke(forward<Function>(f), move(ERR_Unexpected)); });
        }

        template<typename Function>
            requires is_constructible_v<Type, const Type>
        constexpr auto transform_error(Function &&f) const &&
        {
            using Gr   = ExpectedImpl::result_from<Function, const Error &&>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res(in_place, move(Value));
            else
                return _Res([&]() { return invoke(forward<Function>(f), move(ERR_Unexpected)); });
        }

        // equality operators

        template<typename Up, typename _Er2>
            requires(!is_void_v<Up>) && requires(const Type &t, const Up &_unexpected, const Error &e, const _Er2 &e2) {
                { t == _unexpected } -> convertible_to<bool>;
                { e == e2 } -> convertible_to<bool>;
            }
        [[nodiscard]]
        friend constexpr bool operator==(const expected &value, const expected<Up, _Er2> &y) noexcept(
                noexcept(bool(*value == *y)) && noexcept(bool(value.error() == y.error())))
        {
            if (value.has_value() != y.has_value())
                return false;
            if (value.has_value())
                return *value == *y;
            return value.error() == y.error();
        }

        template<typename Up, same_as<Type> old>
            requires(!ExpectedImpl::is_expected_impl<Up>) && requires(const Type &t, const Up &_unexpected) {
                { t == _unexpected } -> convertible_to<bool>;
            }

        [[nodiscard]] friend constexpr bool operator==(const expected<old, Error> &value,
                                                       const Up &val) noexcept(noexcept(bool(*value == val)))
        {
            if (value.has_value())
                return *value == val;
            return false;
        }

        template<typename _Er2>
            requires requires(const Error &e, const _Er2 &e2) {
                { e == e2 } -> convertible_to<bool>;
            }
        [[nodiscard]]
        friend constexpr bool operator==(const expected &value,
                                         const unexpected<_Er2> &e) noexcept(noexcept(bool(value.error() == e.error())))
        {
            if (!value.has_value())
                return value.error() == e.error();
            return false;
        }

        friend constexpr void swap(expected &value, expected &y) noexcept(noexcept(value.swap(y)))
            requires requires { value.swap(y); }
        {
            value.swap(y);
        }

    private:
        template<typename, typename>
        friend class expected;

        template<typename old>
        constexpr void _assign_val(old &&val)
        {
            if (HasValue)
                Value = forward<old>(val);
            else
            {
                ExpectedImpl::reinit(::SFTL::addressof(Value), ::SFTL::addressof(ERR_Unexpected), forward<old>(val));
                HasValue = true;
            }
        }

        template<typename old>
        constexpr void assign_unexpected(old &&val)
        {
            if (HasValue)
            {
                ExpectedImpl::reinit(::SFTL::addressof(ERR_Unexpected), ::SFTL::addressof(Value), forward<old>(val));
                HasValue = false;
            } else
                ERR_Unexpected = forward<old>(val);
        }

        // Swap two expected objects when only one has a value.
        // Precondition: this->HasValue && !rhs.HasValue
        constexpr void _swap_val_unex(expected &rhs) noexcept(
                and_v<is_nothrow_move_constructible<Error>, is_nothrow_move_constructible<Type>>)
        {
            if constexpr (is_nothrow_move_constructible_v<Error>)
            {
                ExpectedImpl::Guard<Error> guard(rhs.ERR_Unexpected);
                construct_at(::SFTL::addressof(rhs.Value),
                             move(Value)); // might throw
                rhs.HasValue = true;
                destroy_at(::SFTL::addressof(Value));
                construct_at(::SFTL::addressof(ERR_Unexpected), guard.release());
                HasValue = false;
            } else
            {
                ExpectedImpl::Guard<Type> guard(Value);
                construct_at(::SFTL::addressof(ERR_Unexpected),
                             move(rhs.ERR_Unexpected)); // might throw
                HasValue = false;
                destroy_at(::SFTL::addressof(rhs.ERR_Unexpected));
                construct_at(::SFTL::addressof(rhs.Value), guard.release());
                rhs.HasValue = true;
            }
        }

        using in_place_inv   = ExpectedImpl::in_place_inv;
        using unexpected_val = ExpectedImpl::unexpected_val;

        template<typename Function>
        explicit constexpr expected(in_place_inv, Function &&fn) : Value(forward<Function>(fn)()), HasValue(true)
        {
        }

        template<typename Function>
        explicit constexpr expected(unexpected_val, Function &&fn) :
            ERR_Unexpected(forward<Function>(fn)()), HasValue(false)
        {
        }

        union
        {
            remove_cv_t<Type> Value;
            Error ERR_Unexpected;
        };

        bool HasValue;
    };

    // Partial specialization for expected<cv void, E>
    template<typename Type, typename Error>
        requires is_void_v<Type>
    class [[nodiscard]] expected<Type, Error>
    {
        static_assert(ExpectedImpl::can_be_inexcpected<Error>);

        template<typename Up, typename error, typename _Unex = unexpected<Error>>
        static constexpr bool from_expected =
                or_v<is_constructible<_Unex, expected<Up, error> &>, is_constructible<_Unex, expected<Up, error>>,
                     is_constructible<_Unex, const expected<Up, error> &>,
                     is_constructible<_Unex, const expected<Up, error>>>;

        template<typename Up>
        static constexpr bool same_value = is_same_v<typename Up::value_type, Type>;

        template<typename Up>
        static constexpr bool same_error = is_same_v<typename Up::error_type, Error>;

    public:
        using value_type      = Type;
        using error_type      = Error;
        using unexpected_type = unexpected<Error>;

        template<typename Up>
        using rebind = expected<Up, error_type>;

        constexpr expected() noexcept : data(), HasValue(true) {}

        expected(const expected &) = default;

        constexpr expected(const expected &value) noexcept(is_nothrow_copy_constructible_v<Error>)
            requires is_copy_constructible_v<Error> && (!is_trivially_copy_constructible_v<Error>)
            : data(), HasValue(value.HasValue)
        {
            if (!HasValue)
                construct_at(::SFTL::addressof(ERR_Unexpected), value.ERR_Unexpected);
        }

        expected(expected &&) = default;

        constexpr expected(expected &&value) noexcept(is_nothrow_move_constructible_v<Error>)
            requires is_move_constructible_v<Error> && (!is_trivially_move_constructible_v<Error>)
            : data(), HasValue(value.HasValue)
        {
            if (!HasValue)
                construct_at(::SFTL::addressof(ERR_Unexpected), move(value).ERR_Unexpected);
        }

        template<typename Up, typename Gr>
            requires is_void_v<Up> && is_constructible_v<Error, const Gr &> && (!from_expected<Up, Gr>)
        constexpr explicit(!is_convertible_v<const Gr &, Error>)
                expected(const expected<Up, Gr> &value) noexcept(is_nothrow_constructible_v<Error, const Gr &>) :
            data(), HasValue(value.HasValue)
        {
            if (!HasValue)
                construct_at(::SFTL::addressof(ERR_Unexpected), value.ERR_Unexpected);
        }

        template<typename Up, typename Gr>
            requires is_void_v<Up> && is_constructible_v<Error, Gr> && (!from_expected<Up, Gr>)
        constexpr explicit(!is_convertible_v<Gr, Error>)
                expected(expected<Up, Gr> &&value) noexcept(is_nothrow_constructible_v<Error, Gr>) :
            data(), HasValue(value.HasValue)
        {
            if (!HasValue)
                construct_at(::SFTL::addressof(ERR_Unexpected), move(value).ERR_Unexpected);
        }

        template<typename Gr = Error>
            requires is_constructible_v<Error, const Gr &>
        constexpr explicit(!is_convertible_v<const Gr &, Error>)
                expected(const unexpected<Gr> &_unexpected) noexcept(is_nothrow_constructible_v<Error, const Gr &>) :
            ERR_Unexpected(_unexpected.error()), HasValue(false)
        {
        }

        template<typename Gr = Error>
            requires is_constructible_v<Error, Gr>
        constexpr explicit(!is_convertible_v<Gr, Error>)
                expected(unexpected<Gr> &&_unexpected) noexcept(is_nothrow_constructible_v<Error, Gr>) :
            ERR_Unexpected(move(_unexpected).error()), HasValue(false)
        {
        }

        constexpr explicit expected(in_place_t) noexcept : expected() {}

        template<typename... _Args>
            requires is_constructible_v<Error, _Args...>
        constexpr explicit expected(unexpect_t, _Args &&...args) noexcept(is_nothrow_constructible_v<Error, _Args...>) :
            ERR_Unexpected(forward<_Args>(args)...), HasValue(false)
        {
        }

        template<typename Up, typename... _Args>
            requires is_constructible_v<Error, initializer_list<Up> &, _Args...>
        constexpr explicit expected(unexpect_t, initializer_list<Up> list, _Args &&...args) noexcept(
                is_nothrow_constructible_v<Error, initializer_list<Up> &, _Args...>) :
            ERR_Unexpected(list, forward<_Args>(args)...), HasValue(false)
        {
        }

        constexpr ~expected() = default;

        constexpr ~expected()
            requires(!is_trivially_destructible_v<Error>)
        {
            if (!HasValue)
                destroy_at(::SFTL::addressof(ERR_Unexpected));
        }

        // assignment

        // Deleted copy assignment, when constraints not met for other overloads
        expected &operator=(const expected &) = delete;

        // Trivial copy assignment
        expected &operator=(const expected &) noexcept(
                and_v<is_nothrow_copy_constructible<Error>, is_nothrow_copy_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_trivial_assign<const Error &>
        = default;

        // Non-trivial copy assignment
        constexpr expected &operator=(const expected &value) noexcept(
                and_v<is_nothrow_copy_constructible<Error>, is_nothrow_copy_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_assign<const Error &>
        {
            if (value.HasValue)
                emplace();
            else
                assign_unexpected(value.ERR_Unexpected);
            return *this;
        }

        // Trivial move assignment
        expected &
        operator=(expected &&) noexcept(and_v<is_nothrow_move_constructible<Error>, is_nothrow_move_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_trivial_assign<Error &&>
        = default;

        // Non-trivial move assignment
        constexpr expected &operator=(expected &&value) noexcept(
                and_v<is_nothrow_move_constructible<Error>, is_nothrow_move_assignable<Error>>)
            requires ExpectedImpl::_unexpectedsable_for_assign<Error &&>
        {
            if (value.HasValue)
                emplace();
            else
                assign_unexpected(move(value.ERR_Unexpected));
            return *this;
        }

        template<typename Gr>
            requires is_constructible_v<Error, const Gr &> && is_assignable_v<Error &, const Gr &>
        constexpr expected &operator=(const unexpected<Gr> &e)
        {
            assign_unexpected(e.error());
            return *this;
        }

        template<typename Gr>
            requires is_constructible_v<Error, Gr> && is_assignable_v<Error &, Gr>
        constexpr expected &operator=(unexpected<Gr> &&e)
        {
            assign_unexpected(move(e.error()));
            return *this;
        }

        // modifiers

        constexpr void emplace() noexcept
        {
            if (!HasValue)
            {
                destroy_at(::SFTL::addressof(ERR_Unexpected));
                HasValue = true;
            }
        }

        constexpr void
        swap(expected &value) noexcept(and_v<is_nothrow_swappable<Error &>, is_nothrow_move_constructible<Error>>)
            requires is_swappable_v<Error> && is_move_constructible_v<Error>
        {
            if (HasValue)
            {
                if (!value.HasValue)
                {
                    construct_at(::SFTL::addressof(ERR_Unexpected),
                                 move(value.ERR_Unexpected)); // might throw
                    destroy_at(::SFTL::addressof(value.ERR_Unexpected));
                    HasValue       = false;
                    value.HasValue = true;
                }
            } else
            {
                if (value.HasValue)
                {
                    construct_at(::SFTL::addressof(value.ERR_Unexpected),
                                 move(ERR_Unexpected)); // might throw
                    destroy_at(::SFTL::addressof(ERR_Unexpected));
                    HasValue       = true;
                    value.HasValue = false;
                } else
                {
                    ::SFTL::swap(ERR_Unexpected, value.ERR_Unexpected);
                }
            }
        }

        // observers

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept
        {
            return HasValue;
        }

        [[nodiscard]]
        constexpr bool has_value() const noexcept
        {
            return HasValue;
        }

        constexpr void operator*() const noexcept {}

        constexpr void value() const &
        {
            static_assert(is_copy_constructible_v<Error>);
            if (HasValue) [[likely]]
                return;
        }

        constexpr void value() &&
        {
            static_assert(is_copy_constructible_v<Error>);
            static_assert(is_move_constructible_v<Error>);
            if (HasValue) [[likely]]
                return;
        }

        constexpr const Error &error() const & noexcept { return ERR_Unexpected; }

        constexpr Error &error() & noexcept { return ERR_Unexpected; }

        constexpr const Error &&error() const && noexcept { return move(ERR_Unexpected); }

        constexpr Error &&error() && noexcept { return move(ERR_Unexpected); }

        template<typename Gr = Error>
        constexpr Error error_or(Gr &&e) const &
        {
            static_assert(is_copy_constructible_v<Error>);
            static_assert(is_convertible_v<Gr, Error>);

            if (HasValue)
                return forward<Gr>(e);
            return ERR_Unexpected;
        }

        template<typename Gr = Error>
        constexpr Error error_or(Gr &&e) &&
        {
            static_assert(is_move_constructible_v<Error>);
            static_assert(is_convertible_v<Gr, Error>);

            if (HasValue)
                return forward<Gr>(e);
            return move(ERR_Unexpected);
        }

        // monadic operations

        template<typename Function>
            requires is_constructible_v<Error, Error &>
        constexpr auto and_then(Function &&f) &
        {
            using Up = ExpectedImpl::result0<Function>;
            static_assert(ExpectedImpl::is_expected_impl<Up>);
            static_assert(is_same_v<typename Up::error_type, Error>);

            if (has_value())
                return invoke(forward<Function>(f));
            else
                return Up(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error &>
        constexpr auto and_then(Function &&f) const &
        {
            using Up = ExpectedImpl::result0<Function>;
            static_assert(ExpectedImpl::is_expected_impl<Up>);
            static_assert(is_same_v<typename Up::error_type, Error>);

            if (has_value())
                return invoke(forward<Function>(f));
            else
                return Up(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, Error>
        constexpr auto and_then(Function &&f) &&
        {
            using Up = ExpectedImpl::result0<Function>;
            static_assert(ExpectedImpl::is_expected_impl<Up>);
            static_assert(is_same_v<typename Up::error_type, Error>);

            if (has_value())
                return invoke(forward<Function>(f));
            else
                return Up(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error>
        constexpr auto and_then(Function &&f) const &&
        {
            using Up = ExpectedImpl::result0<Function>;
            static_assert(ExpectedImpl::is_expected_impl<Up>);
            static_assert(is_same_v<typename Up::error_type, Error>);

            if (has_value())
                return invoke(forward<Function>(f));
            else
                return Up(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
        constexpr auto or_else(Function &&f) &
        {
            using Gr = ExpectedImpl::result<Function, Error &>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>);
            static_assert(is_same_v<typename Gr::value_type, Type>);

            if (has_value())
                return Gr();
            else
                return invoke(forward<Function>(f), ERR_Unexpected);
        }

        template<typename Function>
        constexpr auto or_else(Function &&f) const &
        {
            using Gr = ExpectedImpl::result<Function, const Error &>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>);
            static_assert(is_same_v<typename Gr::value_type, Type>);

            if (has_value())
                return Gr();
            else
                return invoke(forward<Function>(f), ERR_Unexpected);
        }

        template<typename Function>
        constexpr auto or_else(Function &&f) &&
        {
            using Gr = ExpectedImpl::result<Function, Error &&>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>);
            static_assert(is_same_v<typename Gr::value_type, Type>);

            if (has_value())
                return Gr();
            else
                return invoke(forward<Function>(f), move(ERR_Unexpected));
        }

        template<typename Function>
        constexpr auto or_else(Function &&f) const &&
        {
            using Gr = ExpectedImpl::result<Function, const Error &&>;
            static_assert(ExpectedImpl::is_expected_impl<Gr>);
            static_assert(is_same_v<typename Gr::value_type, Type>);

            if (has_value())
                return Gr();
            else
                return invoke(forward<Function>(f), move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Error, Error &>
        constexpr auto transform(Function &&f) &
        {
            using Up   = ExpectedImpl::result0_from<Function>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, forward<Function>(f));
            else
                return _Res(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error &>
        constexpr auto transform(Function &&f) const &
        {
            using Up   = ExpectedImpl::result0_from<Function>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, forward<Function>(f));
            else
                return _Res(unexpect, ERR_Unexpected);
        }

        template<typename Function>
            requires is_constructible_v<Error, Error>
        constexpr auto transform(Function &&f) &&
        {
            using Up   = ExpectedImpl::result0_from<Function>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, forward<Function>(f));
            else
                return _Res(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
            requires is_constructible_v<Error, const Error>
        constexpr auto transform(Function &&f) const &&
        {
            using Up   = ExpectedImpl::result0_from<Function>;
            using _Res = expected<Up, Error>;

            if (has_value())
                return _Res(in_place_inv{}, forward<Function>(f));
            else
                return _Res(unexpect, move(ERR_Unexpected));
        }

        template<typename Function>
        constexpr auto transform_error(Function &&f) &
        {
            using Gr   = ExpectedImpl::result_from<Function, Error &>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res();
            else
                return _Res([&]() { return invoke(forward<Function>(f), ERR_Unexpected); });
        }

        template<typename Function>
        constexpr auto transform_error(Function &&f) const &
        {
            using Gr   = ExpectedImpl::result_from<Function, const Error &>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res();
            else
                return _Res([&]() { return invoke(forward<Function>(f), ERR_Unexpected); });
        }

        template<typename Function>
        constexpr auto transform_error(Function &&f) &&
        {
            using Gr   = ExpectedImpl::result_from<Function, Error &&>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res();
            else
                return _Res([&]() { return invoke(forward<Function>(f), move(ERR_Unexpected)); });
        }

        template<typename Function>
        constexpr auto transform_error(Function &&f) const &&
        {
            using Gr   = ExpectedImpl::result_from<Function, const Error &&>;
            using _Res = expected<Type, Gr>;

            if (has_value())
                return _Res();
            else
                return _Res([&]() { return invoke(forward<Function>(f), move(ERR_Unexpected)); });
        }

        // equality operators

        template<typename Up, typename _Er2>
            requires is_void_v<Up> && requires(const Error &e, const _Er2 &e2) {
                { e == e2 } -> convertible_to<bool>;
            }
        [[nodiscard]]
        friend constexpr bool
        operator==(const expected &value,
                   const expected<Up, _Er2> &y) noexcept(noexcept(bool(value.error() == y.error())))
        {
            if (value.has_value() != y.has_value())
                return false;
            if (value.has_value())
                return true;
            return value.error() == y.error();
        }

        template<typename _Er2>
            requires requires(const Error &e, const _Er2 &e2) {
                { e == e2 } -> convertible_to<bool>;
            }
        [[nodiscard]]
        friend constexpr bool operator==(const expected &value,
                                         const unexpected<_Er2> &e) noexcept(noexcept(bool(value.error() == e.error())))
        {
            if (!value.has_value())
                return value.error() == e.error();
            return false;
        }

        friend constexpr void swap(expected &value, expected &y) noexcept(noexcept(value.swap(y)))
            requires requires { value.swap(y); }
        {
            value.swap(y);
        }

    private:
        template<typename, typename>
        friend class expected;

        template<typename old>
        constexpr void assign_unexpected(old &&val)
        {
            if (HasValue)
            {
                construct_at(::SFTL::addressof(ERR_Unexpected), forward<old>(val));
                HasValue = false;
            } else
                ERR_Unexpected = forward<old>(val);
        }

        using in_place_inv   = ExpectedImpl::in_place_inv;
        using unexpected_val = ExpectedImpl::unexpected_val;

        template<typename Function>
        explicit constexpr expected(in_place_inv, Function &&fn) : data(), HasValue(true)
        {
            forward<Function>(fn)();
        }

        template<typename Function>
        explicit constexpr expected(unexpected_val, Function &&fn) :
            ERR_Unexpected(forward<Function>(fn)()), HasValue(false)
        {
        }

        union
        {
            struct
            {
            } data;
            Error ERR_Unexpected;
        };

        bool HasValue;
    };
} // namespace SFTL
