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

    class partial_ordering
    {
        signed char value;


        [[nodiscard]] constexpr Detail::comptype reverse() const { return static_cast<Detail::comptype>(-value); }

        constexpr explicit partial_ordering(CompareCategory::Order v) noexcept : value(Detail::comptype(v)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<partial_ordering>(partial_ordering) noexcept;
        friend constexpr partial_ordering CompareCategory::make<partial_ordering>(CompareCategory::Order) noexcept;

    public:
        // valid values
        static const partial_ordering less;
        static const partial_ordering equivalent;
        static const partial_ordering greater;
        static const partial_ordering unordered;

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(partial_ordering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(partial_ordering, partial_ordering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(partial_ordering val, literal_zero) noexcept
        {
            return val.value == -1;
        }

        [[nodiscard]] friend constexpr bool operator>(partial_ordering val, literal_zero) noexcept
        {
            return val.value == 1;
        }

        [[nodiscard]] friend constexpr bool operator<=(partial_ordering val, literal_zero) noexcept
        {
            return val.reverse() >= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(partial_ordering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, partial_ordering val) noexcept
        {
            return val.value == 1;
        }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, partial_ordering val) noexcept
        {
            return val.value == -1;
        }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, partial_ordering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, partial_ordering val) noexcept
        {
            return 0 <= val.reverse();
        }

        [[nodiscard]] friend constexpr partial_ordering operator<=>(partial_ordering val, literal_zero) noexcept
        {
            return val;
        }

        [[nodiscard]] friend constexpr partial_ordering operator<=>(literal_zero, partial_ordering val) noexcept
        {
            return partial_ordering(CompareCategory::Order(val.reverse()));
        }
    };

    constexpr partial_ordering partial_ordering::less(CompareCategory::Order::less);
    constexpr partial_ordering partial_ordering::equivalent(CompareCategory::Order::equivalent);
    constexpr partial_ordering partial_ordering::greater(CompareCategory::Order::greater);
    constexpr partial_ordering partial_ordering::unordered(CompareCategory::Order::unordered);

    class weak_ordering
    {
        signed char value;

        constexpr explicit weak_ordering(CompareCategory::Order val) noexcept : value(Detail::comptype(val)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<weak_ordering>(weak_ordering) noexcept;
        friend constexpr weak_ordering CompareCategory::make<weak_ordering>(CompareCategory::Order) noexcept;

    public:
        static const weak_ordering less;
        static const weak_ordering equivalent;
        static const weak_ordering greater;

        [[nodiscard]] constexpr operator partial_ordering() const noexcept
        {
            return CompareCategory::make<partial_ordering>(CompareCategory::Order(value));
        }

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(weak_ordering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(weak_ordering, weak_ordering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(weak_ordering val, literal_zero) noexcept
        {
            return val.value < 0;
        }

        [[nodiscard]] friend constexpr bool operator>(weak_ordering val, literal_zero) noexcept
        {
            return val.value > 0;
        }

        [[nodiscard]] friend constexpr bool operator<=(weak_ordering val, literal_zero) noexcept
        {
            return val.value <= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(weak_ordering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, weak_ordering val) noexcept
        {
            return 0 < val.value;
        }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, weak_ordering val) noexcept
        {
            return 0 > val.value;
        }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, weak_ordering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, weak_ordering val) noexcept
        {
            return 0 >= val.value;
        }

        [[nodiscard]] friend constexpr weak_ordering operator<=>(weak_ordering val, literal_zero) noexcept
        {
            return val;
        }

        [[nodiscard]] friend constexpr weak_ordering operator<=>(literal_zero, weak_ordering val) noexcept
        {
            return weak_ordering(CompareCategory::Order(-val.value));
        }
    };

    constexpr weak_ordering weak_ordering::less(CompareCategory::Order::less);
    constexpr weak_ordering weak_ordering::equivalent(CompareCategory::Order::equivalent);
    constexpr weak_ordering weak_ordering::greater(CompareCategory::Order::greater);

    class strong_ordering
    {
        signed char value;

        constexpr explicit strong_ordering(CompareCategory::Order val) noexcept : value(Detail::comptype(val)) {}

        friend constexpr CompareCategory::Order CompareCategory::ord<strong_ordering>(strong_ordering) noexcept;
        friend constexpr strong_ordering CompareCategory::make<strong_ordering>(CompareCategory::Order) noexcept;

    public:
        // valid values
        static const strong_ordering less;
        static const strong_ordering equal;
        static const strong_ordering equivalent;
        static const strong_ordering greater;

        [[nodiscard]] constexpr operator partial_ordering() const noexcept
        {
            return CompareCategory::make<partial_ordering>(CompareCategory::Order(value));
        }

        [[nodiscard]] constexpr operator weak_ordering() const noexcept
        {
            return CompareCategory::make<weak_ordering>(CompareCategory::Order(value));
        }

        // comparisons
        [[nodiscard]] friend constexpr bool operator==(strong_ordering val, literal_zero) noexcept
        {
            return val.value == 0;
        }

        [[nodiscard]] friend constexpr bool operator==(strong_ordering, strong_ordering) noexcept = default;

        [[nodiscard]] friend constexpr bool operator<(strong_ordering val, literal_zero) noexcept
        {
            return val.value < 0;
        }

        [[nodiscard]] friend constexpr bool operator>(strong_ordering val, literal_zero) noexcept
        {
            return val.value > 0;
        }

        [[nodiscard]] friend constexpr bool operator<=(strong_ordering val, literal_zero) noexcept
        {
            return val.value <= 0;
        }

        [[nodiscard]] friend constexpr bool operator>=(strong_ordering val, literal_zero) noexcept
        {
            return val.value >= 0;
        }

        [[nodiscard]] friend constexpr bool operator<(literal_zero, strong_ordering val) noexcept
        {
            return 0 < val.value;
        }

        [[nodiscard]] friend constexpr bool operator>(literal_zero, strong_ordering val) noexcept
        {
            return 0 > val.value;
        }

        [[nodiscard]] friend constexpr bool operator<=(literal_zero, strong_ordering val) noexcept
        {
            return 0 <= val.value;
        }

        [[nodiscard]] friend constexpr bool operator>=(literal_zero, strong_ordering val) noexcept
        {
            return 0 >= val.value;
        }

        [[nodiscard]] friend constexpr strong_ordering operator<=>(strong_ordering val, literal_zero) noexcept
        {
            return val;
        }

        [[nodiscard]] friend constexpr strong_ordering operator<=>(literal_zero, strong_ordering val) noexcept
        {
            return strong_ordering(CompareCategory::Order(-val.value));
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
        inline constexpr unsigned CompareCategoryId<partial_ordering> = 2;
        template<>
        inline constexpr unsigned CompareCategoryId<weak_ordering> = 4;
        template<>
        inline constexpr unsigned CompareCategoryId<strong_ordering> = 8;

        template<typename... T>
        constexpr auto common_compare_category()
        {
            if constexpr (constexpr unsigned category = (CompareCategoryId<T> | ...); category & 1)
                return;
            else if constexpr (bool(category & CompareCategoryId<partial_ordering>))
                return partial_ordering::equivalent;
            else if constexpr (bool(category & CompareCategoryId<weak_ordering>))
                return weak_ordering::equivalent;
            else
                return strong_ordering::equivalent;
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
    struct common_comparison_category<partial_ordering>
    {
        using type = partial_ordering;
    };

    template<>
    struct common_comparison_category<weak_ordering>
    {
        using type = weak_ordering;
    };

    template<>
    struct common_comparison_category<strong_ordering>
    {
        using type = strong_ordering;
    };

    template<>
    struct common_comparison_category<>
    {
        using type = strong_ordering;
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

    constexpr strong_ordering strong_ordering::less(CompareCategory::Order::less);
    constexpr strong_ordering strong_ordering::equal(CompareCategory::Order::equivalent);
    constexpr strong_ordering strong_ordering::equivalent(CompareCategory::Order::equivalent);
    constexpr strong_ordering strong_ordering::greater(CompareCategory::Order::greater);


    [[nodiscard]] constexpr bool is_eq(partial_ordering cmp) noexcept { return cmp == nullptr; }
    [[nodiscard]] constexpr bool is_neq(partial_ordering cmp) noexcept { return cmp != nullptr; }
    [[nodiscard]] constexpr bool is_lt(partial_ordering cmp) noexcept { return cmp < nullptr; }
    [[nodiscard]] constexpr bool is_lteq(partial_ordering cmp) noexcept { return cmp <= nullptr; }
    [[nodiscard]] constexpr bool is_gt(partial_ordering cmp) noexcept { return cmp > nullptr; }
    [[nodiscard]] constexpr bool is_gteq(partial_ordering cmp) noexcept { return cmp >= nullptr; }

    template<typename Type, typename Category = partial_ordering>
    concept three_way_comparable =
            Detail::weakly_eq_compare_with<Type, Type> && Detail::partially_ordered_with<Type, Type> &&
            requires(const remove_reference_t<Type> &A, const remove_reference_t<Type> &B) {
                { A <=> B } -> Detail::compares_as<Category>;
            };

    template<typename Type, typename U, typename Category = partial_ordering>
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
                        return weak_ordering::less;
                    if (u < t)
                        return weak_ordering::greater;
                    return weak_ordering::equivalent;
                }
            }
        } synth3way = {};

    } // namespace Detail

    template<typename Type, typename Arb = Type>
    using synthesize3way_type = decltype(Detail::synth3way(declval<Type &>(), declval<Arb &>()));

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
                {
                    if (pt < pu)
                        return strong_ordering::less;
                    if (pt > pu)
                        return strong_ordering::greater;
                    return strong_ordering::equal;
                }

                const auto it = reinterpret_cast<uintptr_type>(pt);
                const auto iu = reinterpret_cast<uintptr_type>(pu);

                if (it < iu)
                    return strong_ordering::less;
                if (it > iu)
                    return strong_ordering::greater;
                return strong_ordering::equal;
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
            strong_ordering cmp;
        };

        constexpr MinCmpResult min_cmp(ptrdiff_t d1, ptrdiff_t d2) noexcept
        {
            if (d1 < d2)
                return {d1, strong_ordering::less};
            if (d1 > d2)
                return {d2, strong_ordering::greater};
            return {d1, strong_ordering::equal};
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
                            return c < 0 ? strong_ordering::less : strong_ordering::greater;
                    }
                    return lencmp;
                }
            }
        }

        while (first1 != last1)
        {
            if (first2 == last2)
                return strong_ordering::greater;
            if (auto cmp = comp(*first1, *first2); cmp != 0)
                return cmp;
            ++first1;
            ++first2;
        }
        return (first2 == last2) ? strong_ordering::equal : strong_ordering::less;
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
