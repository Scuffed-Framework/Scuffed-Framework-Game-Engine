#pragma once
#include "../Compare.hpp"
#include "../Iterators.hpp"
#include "../Streams/BasicOut.hpp"
#include "../TypeTraits.hpp"
#include "../Types.hpp"

namespace SFTL
{
    template<typename T, size_type N>
    struct array
    {
        using value_type             = T;
        using size_type              = ::SFTL::size_type;
        using difference_type        = ptrdiff_t;
        using reference              = T &;
        using const_reference        = const T &;
        using pointer                = T *;
        using const_pointer          = const T *;
        using iterator               = T *;
        using const_iterator         = const T *;
        using reverse_iterator       = ::SFTL::reverse_iterator<iterator>;
        using const_reverse_iterator = ::SFTL::reverse_iterator<const_iterator>;

        // Stored as a public raw array to preserve aggregate initialization rules
        T elements[N];

        // Iterators
        [[nodiscard]] constexpr iterator begin() noexcept { return elements; }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return elements; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return elements; }

        [[nodiscard]] constexpr iterator end() noexcept { return elements + N; }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return elements + N; }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return elements + N; }

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept
        {
            return const_reverse_iterator(begin());
        }

        // Capacity
        [[nodiscard]] constexpr size_type size() const noexcept { return N; }
        [[nodiscard]] constexpr size_type max_size() const noexcept { return N; }
        [[nodiscard]] constexpr bool empty() const noexcept { return N == 0; }

        // Element access
        [[nodiscard]] constexpr reference operator[](size_type sz) noexcept { return elements[sz]; }

        [[nodiscard]] constexpr const_reference operator[](size_type sz) const noexcept { return elements[sz]; }

        [[nodiscard]] constexpr reference at(size_type sz)
        {
            if (sz >= N)
                cout << "array::at: index out of range";
            return elements[sz];
        }

        [[nodiscard]] constexpr const_reference at(size_type sz) const
        {
            if (sz >= N)
                cout << "array::at: index out of range";
            return elements[sz];
        }

        [[nodiscard]] constexpr reference front() noexcept
        {
            static_assert(N > 0, "array::front() on empty array");
            return elements[0];
        }

        [[nodiscard]] constexpr const_reference front() const noexcept
        {
            static_assert(N > 0, "array::front() on empty array");
            return elements[0];
        }

        [[nodiscard]] constexpr reference back() noexcept
        {
            static_assert(N > 0, "array::back() on empty array");
            return elements[N - 1];
        }

        [[nodiscard]] constexpr const_reference back() const noexcept
        {
            static_assert(N > 0, "array::back() on empty array");
            return elements[N - 1];
        }

        [[nodiscard]] constexpr pointer data() noexcept { return elements; }
        [[nodiscard]] constexpr const_pointer data() const noexcept { return elements; }

        // Operations
        constexpr void fill(const T &type)
        {
            for (size_type size = 0; size < N; ++size)
                elements[size] = type;
        }

        constexpr void swap(array &other) noexcept(is_nothrow_swappable_v<T>)
        {
            for (size_type size = 0; size < N; ++size)
            {
                using SFTL::swap;
                swap(elements[size], other.elements[size]);
            }
        }
    };

    // Specialization for N == 0 (Zero-size array support)
    template<typename T>
    struct array<T, 0>
    {
        using value_type             = T;
        using size_type              = ::SFTL::size_type;
        using difference_type        = ptrdiff_t;
        using reference              = T &;
        using const_reference        = const T &;
        using pointer                = T *;
        using const_pointer          = const T *;
        using iterator               = T *;
        using const_iterator         = const T *;
        using reverse_iterator       = ::SFTL::reverse_iterator<iterator>;
        using const_reverse_iterator = ::SFTL::reverse_iterator<const_iterator>;

        [[nodiscard]] constexpr iterator begin() noexcept { return nullptr; }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return nullptr; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return nullptr; }

        [[nodiscard]] constexpr iterator end() noexcept { return nullptr; }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return nullptr; }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return nullptr; }

        [[nodiscard]] constexpr size_type size() const noexcept { return 0; }
        [[nodiscard]] constexpr size_type max_size() const noexcept { return 0; }
        [[nodiscard]] constexpr bool empty() const noexcept { return true; }

        [[nodiscard]] constexpr reference operator[](size_type) noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr const_reference operator[](size_type) const noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr reference at(size_type) { cout << "array::at: index out of range"; }

        [[nodiscard]] constexpr const_reference at(size_type) const { cout << "array::at: index out of range"; }

        [[nodiscard]] constexpr reference front() noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr const_reference front() const noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr reference back() noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr const_reference back() const noexcept { __builtin_unreachable(); }

        [[nodiscard]] constexpr pointer data() noexcept { return nullptr; }
        [[nodiscard]] constexpr const_pointer data() const noexcept { return nullptr; }

        constexpr void fill(const T &) {}

        constexpr void swap(array &) noexcept {}
    };

    // Comparison Operators
    template<typename T, size_type N>
    [[nodiscard]] constexpr bool operator==(const array<T, N> &x, const array<T, N> &y)
    {
        for (size_type size = 0; size < N; ++size)
        {
            if (!(x[size] == y[size]))
                return false;
        }
        return true;
    }

    template<typename T, size_type N>
    [[nodiscard]] constexpr auto operator<=>(const array<T, N> &x, const array<T, N> &y)
    {
        for (size_type size = 0; size < N; ++size)
        {
            if (auto cmp = x[size] <=> y[size]; cmp != 0)
                return cmp;
        }
        return strong_ordering::equal;
    }

    // Deduction Guides
    template<typename T, typename... U>
    array(T, U...) -> array<T, sizeof...(U) + 1>;

    // C++23 to_array helper
    template<typename T, size_type N, typename U = remove_cv_t<T>>
    [[nodiscard]] constexpr array<U, N> to_array(T (&a)[N])
    {
        return [&]<size_type... size>(index_sequence<size...>)
        { return array<U, N>{a[size]...}; }(make_index_sequence<N>{});
    }
} // namespace SFTL
