#pragma once
#include "Char.hpp"
#include "Containers/InitializerList.hpp"
#include "Iterators.hpp"
#include "Operations.hpp"
#include "TypeTraits.hpp"
#include "UsefulMacros.hpp"

namespace SFTL
{
    template<typename Type>
    NO_DISCARD inline constexpr auto begin(Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto begin(const Type &t) noexcept(noexcept(t.begin())) -> decltype(t.begin())
    {
        return t.begin();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto end(Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto end(const Type &t) noexcept(noexcept(t.end())) -> decltype(t.end())
    {
        return t.end();
    }

    template<typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr T *begin(T (&arr)[num]) noexcept
    {
        return arr;
    }

    template<typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr T *end(T (&arr)[num]) noexcept
    {
        return arr + num;
    }

    template<typename Type>
    NO_DISCARD constexpr auto scbegin(const Type &t) noexcept(noexcept(begin(t))) -> decltype(begin(t))
    {
        return begin(t);
    }

    template<typename Type>
    NO_DISCARD constexpr auto end(const Type &t) noexcept(noexcept(end(t))) -> decltype(end(t))
    {
        return end(t);
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto rbegin(Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto rbegin(const Type &t) noexcept(noexcept(t.rbegin())) -> decltype(t.rbegin())
    {
        return t.rbegin();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto rend(Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto rend(const Type &t) noexcept(noexcept(t.rend())) -> decltype(t.rend())
    {
        return t.rend();
    }

    template<typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr reverse_iterator<T *> rbegin(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr + num);
    }

    template<typename T, ::SFTL::size_type num>
    NO_DISCARD inline constexpr ::SFTL::reverse_iterator<T *> rend(T (&arr)[num]) noexcept
    {
        return reverse_iterator<T *>(arr);
    }

    template<typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rbegin(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.end());
    }

    template<typename T>
    NO_DISCARD inline constexpr reverse_iterator<const T *> rend(initializer_list<T> list) noexcept
    {
        return reverse_iterator<const T *>(list.begin());
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto crbegin(const Type &t) noexcept(noexcept(rbegin(t))) -> decltype(rbegin(t))
    {
        return rbegin(t);
    }

    template<typename Type>
    NO_DISCARD inline constexpr auto crend(const Type &t) noexcept(noexcept(rend(t))) -> decltype(rend(t))
    {
        return rend(t);
    }

    template<typename T, ptrdiff_t v>
    NO_DISCARD constexpr ptrdiff_t ssize(const T (&)[v]) noexcept
    {
        return v;
    }

    struct literal_zero
    {
        consteval literal_zero(literal_zero *) noexcept {}
    };
    namespace Detail
    {
        using comptype = signed char;
    }

    namespace CompareCategory
    {
        enum class Order : signed char
        {
            equivalent = 0,
            less       = -1,
            greater    = 1,
            unordered  = -__SCHAR_MAX__ - 1
        };
        template<typename Ordering>
        inline constexpr Ordering make(CompareCategory::Order o) noexcept
        {
            return Ordering(o);
        }

        template<typename Ordering>
        inline constexpr CompareCategory::Order ord(Ordering o) noexcept
        {
            return CompareCategory::Order(o.value);
        }

    } // namespace CompareCategory

    class partialOrdering
    {
        signed char value;


        [[nodiscard]] constexpr Detail::comptype reverse() const { return static_cast<Detail::comptype>(-value); }

        constexpr explicit partialOrdering(CompareCategory::Order v) noexcept : value(Detail::comptype(v)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<partialOrdering>(partialOrdering) noexcept;
        friend constexpr partialOrdering CompareCategory::make<partialOrdering>(CompareCategory::Order) noexcept;

    public:
        // valid values
        static const partialOrdering less;
        static const partialOrdering equivalent;
        static const partialOrdering greater;
        static const partialOrdering unordered;

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(partialOrdering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(partialOrdering, partialOrdering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(partialOrdering val, literal_zero) noexcept
        {
            return val.value == -1;
        }

        [[nodiscard]] friend constexpr bool operator>(partialOrdering val, literal_zero) noexcept
        {
            return val.value == 1;
        }

        [[nodiscard]] friend constexpr bool operator<=(partialOrdering val, literal_zero) noexcept
        {
            return val.reverse() >= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(partialOrdering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, partialOrdering val) noexcept
        {
            return val.value == 1;
        }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, partialOrdering val) noexcept
        {
            return val.value == -1;
        }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, partialOrdering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, partialOrdering val) noexcept
        {
            return 0 <= val.reverse();
        }

        [[nodiscard]] friend constexpr partialOrdering operator<=>(partialOrdering val, literal_zero) noexcept
        {
            return val;
        }

        [[nodiscard]] friend constexpr partialOrdering operator<=>(literal_zero, partialOrdering val) noexcept
        {
            return partialOrdering(CompareCategory::Order(val.reverse()));
        }
    };

    constexpr partialOrdering partialOrdering::less(CompareCategory::Order::less);
    constexpr partialOrdering partialOrdering::equivalent(CompareCategory::Order::equivalent);
    constexpr partialOrdering partialOrdering::greater(CompareCategory::Order::greater);
    constexpr partialOrdering partialOrdering::unordered(CompareCategory::Order::unordered);

    class weakOrdering
    {
        signed char value;

        constexpr explicit weakOrdering(CompareCategory::Order val) noexcept : value(Detail::comptype(val)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<weakOrdering>(weakOrdering) noexcept;
        friend constexpr weakOrdering CompareCategory::make<weakOrdering>(CompareCategory::Order) noexcept;

    public:
        static const weakOrdering less;
        static const weakOrdering equivalent;
        static const weakOrdering greater;

        [[nodiscard]] constexpr operator partialOrdering() const noexcept
        {
            return CompareCategory::make<partialOrdering>(CompareCategory::Order(value));
        }

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(weakOrdering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(weakOrdering, weakOrdering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(weakOrdering val, literal_zero) noexcept { return val.value < 0; }

        [[nodiscard]] friend constexpr bool operator>(weakOrdering val, literal_zero) noexcept { return val.value > 0; }

        [[nodiscard]] friend constexpr bool operator<=(weakOrdering val, literal_zero) noexcept
        {
            return val.value <= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(weakOrdering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, weakOrdering val) noexcept { return 0 < val.value; }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, weakOrdering val) noexcept { return 0 > val.value; }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, weakOrdering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, weakOrdering val) noexcept
        {
            return 0 >= val.value;
        }

        [[nodiscard]] friend constexpr weakOrdering operator<=>(weakOrdering val, literal_zero) noexcept { return val; }

        [[nodiscard]] friend constexpr weakOrdering operator<=>(literal_zero, weakOrdering val) noexcept
        {
            return weakOrdering(CompareCategory::Order(-val.value));
        }
    };

    constexpr weakOrdering weakOrdering::less(CompareCategory::Order::less);
    constexpr weakOrdering weakOrdering::equivalent(CompareCategory::Order::equivalent);
    constexpr weakOrdering weakOrdering::greater(CompareCategory::Order::greater);

    class strongOrdering
    {
        signed char value;

        constexpr explicit strongOrdering(CompareCategory::Order val) noexcept : value(Detail::comptype(val)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<strongOrdering>(strongOrdering) noexcept;
        friend constexpr strongOrdering CompareCategory::make<strongOrdering>(CompareCategory::Order) noexcept;

    public:
        // valid values
        static const strongOrdering less;
        static const strongOrdering equal;
        static const strongOrdering equivalent;
        static const strongOrdering greater;

        [[nodiscard]] constexpr operator partialOrdering() const noexcept
        {
            return CompareCategory::make<partialOrdering>(CompareCategory::Order(value));
        }

        [[nodiscard]] constexpr operator weakOrdering() const noexcept
        {
            return CompareCategory::make<weakOrdering>(CompareCategory::Order(value));
        }

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(strongOrdering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(strongOrdering, strongOrdering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(strongOrdering val, literal_zero) noexcept
        {
            return val.value < 0;
        }

        [[nodiscard]] friend constexpr bool operator>(strongOrdering val, literal_zero) noexcept
        {
            return val.value > 0;
        }

        [[nodiscard]] friend constexpr bool operator<=(strongOrdering val, literal_zero) noexcept
        {
            return val.value <= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(strongOrdering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, strongOrdering val) noexcept
        {
            return 0 < val.value;
        }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, strongOrdering val) noexcept
        {
            return 0 > val.value;
        }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, strongOrdering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, strongOrdering val) noexcept
        {
            return 0 >= val.value;
        }

        [[nodiscard]] friend constexpr strongOrdering operator<=>(strongOrdering val, literal_zero) noexcept
        {
            return val;
        }

        [[nodiscard]] friend constexpr strongOrdering operator<=>(literal_zero, strongOrdering val) noexcept
        {
            return strongOrdering(CompareCategory::Order(-val.value));
        }
    };

    template<typename T>
    concept boolean_testable_impl = convertible_to<T, bool>;

    template<typename T>
    concept boolean_testable = boolean_testable_impl<T> && requires(T &&t) {
        { !forward<T>(t) } -> boolean_testable_impl;
    };


    namespace Detail
    {
        template<typename Type>
        inline constexpr unsigned CompareCategoryId = 1;
        template<>
        inline constexpr unsigned CompareCategoryId<partialOrdering> = 2;
        template<>
        inline constexpr unsigned CompareCategoryId<weakOrdering> = 4;
        template<>
        inline constexpr unsigned CompareCategoryId<strongOrdering> = 8;

        template<typename... T>
        constexpr auto common_compare_category()
        {
            if constexpr (constexpr unsigned category = (CompareCategoryId<T> | ...); category & 1)
                return;
            else if constexpr (bool(category & CompareCategoryId<partialOrdering>))
                return partialOrdering::equivalent;
            else if constexpr (bool(category & CompareCategoryId<weakOrdering>))
                return weakOrdering::equivalent;
            else
                return strongOrdering::equivalent;
        }
    } // namespace Detail

    template<typename... T>
    struct common_comparison_category
    {
        using type = decltype(Detail::common_compare_category<T...>());
    };
    template<typename Type>
    struct common_comparison_category<Type>
    {
        using type = void;
    };

    template<>
    struct common_comparison_category<partialOrdering>
    {
        using type = partialOrdering;
    };

    template<>
    struct common_comparison_category<weakOrdering>
    {
        using type = weakOrdering;
    };

    template<>
    struct common_comparison_category<strongOrdering>
    {
        using type = strongOrdering;
    };

    template<>
    struct common_comparison_category<>
    {
        using type = strongOrdering;
    };

    template<typename... T>
    using common_comparison_category_t = typename common_comparison_category<T...>::type;

    namespace Detail
    {
        template<typename Type>
        using CREF = const remove_reference_t<Type> &;

        template<typename Type, typename U>
        concept weakly_eq_compare_with = requires(CREF<Type> t, CREF<U> u) {
            { t == u } -> boolean_testable;
            { t != u } -> boolean_testable;
            { u == t } -> boolean_testable;
            { u != t } -> boolean_testable;
        };
        template<typename Type, typename U>
        concept partially_ordered_with = requires(const remove_reference_t<Type> &ty, const remove_reference_t<U> &u) {
            { ty < u } -> boolean_testable;
            { ty > u } -> boolean_testable;
            { ty <= u } -> boolean_testable;
            { ty >= u } -> boolean_testable;
            { u < ty } -> boolean_testable;
            { u > ty } -> boolean_testable;
            { u <= ty } -> boolean_testable;
            { u >= ty } -> boolean_testable;
        };
        template<typename T, typename Category>
        concept compares_as = same_as<common_comparison_category_t<T, Category>, Category>;

        template<typename Type, typename U, typename cref = common_reference_t<const Type &, const U &>>
        concept comparison_common_type_with_impl =
                same_as<common_reference_t<const Type &, const U &>, common_reference_t<const U &, const Type &>> &&
                requires {
                    requires convertible_to<const Type &, const cref &> || convertible_to<Type, const cref &>;
                    requires convertible_to<const U &, const cref &> || convertible_to<U, const cref &>;
                };

        template<typename T, typename U>
        concept comparison_common_type_with = comparison_common_type_with_impl<remove_cvref_t<T>, remove_cvref_t<U>>;
    } // namespace Detail

    constexpr strongOrdering strongOrdering::less(CompareCategory::Order::less);
    constexpr strongOrdering strongOrdering::equal(CompareCategory::Order::equivalent);
    constexpr strongOrdering strongOrdering::equivalent(CompareCategory::Order::equivalent);
    constexpr strongOrdering strongOrdering::greater(CompareCategory::Order::greater);


    [[nodiscard]] constexpr bool is_eq(partialOrdering cmp) noexcept { return cmp == nullptr; }
    [[nodiscard]] constexpr bool is_neq(partialOrdering cmp) noexcept { return cmp != nullptr; }
    [[nodiscard]] constexpr bool is_lt(partialOrdering cmp) noexcept { return cmp < nullptr; }
    [[nodiscard]] constexpr bool is_lteq(partialOrdering cmp) noexcept { return cmp <= nullptr; }
    [[nodiscard]] constexpr bool is_gt(partialOrdering cmp) noexcept { return cmp > nullptr; }
    [[nodiscard]] constexpr bool is_gteq(partialOrdering cmp) noexcept { return cmp >= nullptr; }

    template<typename Type, typename Category = partialOrdering>
    concept three_way_comparable =
            Detail::weakly_eq_compare_with<Type, Type> && Detail::partially_ordered_with<Type, Type> &&
            requires(const remove_reference_t<Type> &A, const remove_reference_t<Type> &B) {
                { A <=> B } -> Detail::compares_as<Category>;
            };

    template<typename Type, typename U, typename Category = partialOrdering>
    concept three_way_comparable_with =
            three_way_comparable<Type, Category> && three_way_comparable<U, Category> &&
            Detail::comparison_common_type_with<Type, U> &&
            three_way_comparable<common_reference_t<const remove_reference_t<Type> &, const remove_reference_t<U> &>,
                                 Category> &&
            Detail::weakly_eq_compare_with<Type, U> && Detail::partially_ordered_with<Type, U> &&
            requires(const remove_reference_t<Type> &t, const remove_reference_t<U> &u) {
                { t <=> u } -> Detail::compares_as<Category>;
                { u <=> t } -> Detail::compares_as<Category>;
            };


    namespace Detail
    {
        template<typename Type, typename Arb>
        concept not_overloaded_spaceship = ! requires(Type&& type, Arb&& arb)
        {
            operator<=>(static_cast<Type&&>(type), static_cast<Arb&&>(arb));
        } && ! requires(Type&& type, Arb&& arb)
        {
            static_cast<Type&&>(type).operator<=>(static_cast<Arb&&>(arb));
        } && (is_same_v<Type, Arb> || (! requires(Type&& type, Arb&& arb)
        {
            operator<=>(static_cast<Arb&&>(arb), static_cast<Type&&>(type));
        } && ! requires(Type&& type, Arb&& arb)
        {
            static_cast<Arb&&>(arb).operator<=>(static_cast<Type&&>(type));
        }));

        template<typename Type, typename Arb>
        concept three_way_builtin_ptr_cmp =
                requires(Type &&t, Arb &&u) { static_cast<Type &&>(t) <=> static_cast<Arb &&>(u); } &&
                convertible_to<Type, const volatile void *> && convertible_to<Arb, const volatile void *> &&
                not_overloaded_spaceship<Type, Arb>;

        inline constexpr struct SynthesizeThreeWay
        {
            template<typename Type, typename Arb>
            static constexpr bool noexception(const Type *t = nullptr, const Arb *u = nullptr)
            {
                if constexpr (three_way_comparable_with<Type, Arb>)
                    return noexcept(*t <=> *u);
                else
                    return noexcept(*t < *u) && noexcept(*u < *t);
            }

            template<typename Type, typename Arb>
            [[nodiscard]] constexpr auto operator()(const Type &t, const Arb &u) const
                    noexcept(noexception<Type, Arb>())
                requires requires {
                    { t < u } -> boolean_testable;
                    { u < t } -> boolean_testable;
                }
            {
                if constexpr (three_way_comparable_with<Type, Arb>)
                    return t <=> u;
                else
                {
                    if (t < u)
                        return weakOrdering::less;
                    if (u < t)
                        return weakOrdering::greater;
                    return weakOrdering::equivalent;
                }
            }
        } synth3way = {};

    } // namespace Detail

    template<typename Type, typename Arb = Type>
    using synth3way_t = decltype(Detail::synth3way(declval<Type &>(), declval<Arb &>()));

    struct compare_three_way
    {
        template<typename Type, typename Arb>
        constexpr auto operator()
                [[nodiscard]] (Type &&t, Arb &&u) noexcept(noexcept(declval<Type>() <=> declval<Arb>()))
        {
            if constexpr (Detail::three_way_builtin_ptr_cmp<Type, Arb>)
            {
                auto pt = static_cast<const volatile void *>(t);
                auto pu = static_cast<const volatile void *>(u);
                if (is_constant_evaluated())
                    return pt <=> pu;
                const auto it = reinterpret_cast<uintptr_type>(pt);
                const auto iu = reinterpret_cast<uintptr_type>(pu);
                return it <=> iu;
            } else
                return static_cast<Type &&>(t) <=> static_cast<Arb &&>(u);
        }
        using is_transparent = void;
    };

    namespace Detail
    {
        // replacements for std::iter_value_t / std::contiguous_iterator
        template<typename Iter>
        using iter_value_t = remove_cvref_t<decltype(*declval<Iter &>())>;

        // Simplified on purpose: the memcmp fast-path only ever matters for
        // raw-pointer-backed contiguous storage (arrays, SFTL::vector's iterator).
        // If you later add a wrapped contiguous iterator type, extend this with
        // an iterator_concept tag check instead of widening it silently.
        template<typename Iter>
        concept contiguous_iterator_ = is_pointer_v<remove_cvref_t<Iter>>;

        struct MinCmpResult
        {
            ptrdiff_t len;
            strongOrdering cmp;
        };

        constexpr MinCmpResult min_cmp(ptrdiff_t d1, ptrdiff_t d2) noexcept
        {
            if (d1 < d2)
                return {d1, strongOrdering::less};
            if (d1 > d2)
                return {d2, strongOrdering::greater};
            return {d1, strongOrdering::equal};
        }
    } // namespace Detail

    template<typename Type>
    struct is_memcmp_ordered
    {
        static constexpr bool val = []
        {
            if constexpr (is_integral_v<Type>)
                return sizeof(Type) == 1 && Type(-1) > Type(1); // single-byte unsigned only
            else
                return false;
        }();
    };

    template<typename Type, typename Arb, bool = sizeof(Type) == sizeof(Arb)>
    struct is_memcmp_ordered_with
    {
        static constexpr bool val = is_memcmp_ordered<Type>::val && is_memcmp_ordered<Arb>::val;
    };

    template<typename Type, typename Arb>
    struct is_memcmp_ordered_with<Type, Arb, false>
    {
        static constexpr bool val = false;
    };

    template<typename Iterator1, typename Iterator2>
    concept memcmp_ordered_with =
            is_memcmp_ordered_with<Detail::iter_value_t<Iterator1>, Detail::iter_value_t<Iterator2>>::val &&
            Detail::contiguous_iterator_<Iterator1> && Detail::contiguous_iterator_<Iterator2>;

    template<typename InputIterator1, typename InputIterator2, typename C>
    [[nodiscard]] constexpr auto lexicographical_compare_three_way(InputIterator1 first1, InputIterator1 last1,
                                                                   InputIterator2 first2, InputIterator2 last2, C comp)
            -> decltype(comp(*first1, *first2))
    {
        using Category = decltype(comp(*first1, *first2));
        static_assert(same_as<common_comparison_category_t<Category>, Category>);

        if (!is_constant_evaluated())
        {
            if constexpr (same_as<C, Detail::SynthesizeThreeWay> || same_as<C, compare_three_way>)
            {
                if constexpr (memcmp_ordered_with<InputIterator1, InputIterator2>)
                {
                    const auto [len, lencmp] = Detail::min_cmp(last1 - first1, last2 - first2);
                    if (len)
                    {
                        const auto blen = static_cast<size_type>(len) * sizeof(*first1);
                        const int c     = ::SFTL::memcmp(&*first1, &*first2, blen);
                        if (c != 0)
                            return c < 0 ? strongOrdering::less : strongOrdering::greater;
                    }
                    return lencmp;
                }
            }
        }

        while (first1 != last1)
        {
            if (first2 == last2)
                return strongOrdering::greater;
            if (auto cmp = comp(*first1, *first2); cmp != 0)
                return cmp;
            ++first1;
            ++first2;
        }
        return (first2 == last2) ? strongOrdering::equal : strongOrdering::less;
    }

    template<typename InputIterator1, typename InputIterator2>
    constexpr auto lexicographical_compare_three_way(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2,
                                                     InputIterator2 last2)
    {

        return lexicographical_compare_three_way(first1, last1, first2, last2, compare_three_way{});
    }

    namespace Detail
    {
        template<typename Type, typename Arb>
        using three_way_result_t = decltype(declval<Type &>() <=> declval<Arb &>());
    }

    template<typename Type, typename Arb = Type>
    struct compare_three_way_result
    {
    };

    template<typename Type, typename Arb>
        requires requires { typename Detail::three_way_result_t<Type, Arb>; }
    struct compare_three_way_result<Type, Arb>
    {
        using type = Detail::three_way_result_t<Type, Arb>;
    };

    template<typename Type, typename Arb = Type>
    using compare_three_way_result_t = typename compare_three_way_result<Type, Arb>::type;
} // namespace SFTL
