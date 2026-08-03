#pragma once
#include "Types.hpp"
namespace SFTL
{
    template <class Iterator>
    class reverse_iterator
    {
    public:
        using iterator_type = Iterator;

        constexpr reverse_iterator() = default;

        constexpr explicit reverse_iterator(Iterator it)
            : current_(it)
        {
        }

        // Allows converting a reverse_iterator over a convertible base iterator
        // (e.g. reverse_iterator<T*> -> reverse_iterator<const T*>).
        template <class Other>
        constexpr reverse_iterator(const reverse_iterator<Other> &other)
            : current_(other.base())
        {
        }

        [[nodiscard]] constexpr Iterator base() const
        {
            return current_;
        }

        [[nodiscard]] constexpr decltype(auto) operator*() const
        {
            Iterator tmp = current_;
            --tmp;
            return *tmp;
        }

        [[nodiscard]] constexpr auto operator->() const
        {
            Iterator tmp = current_;
            --tmp;
            return &(*tmp);
        }

        constexpr reverse_iterator &operator++()
        {
            --current_;
            return *this;
        }

        constexpr reverse_iterator operator++(int)
        {
            reverse_iterator tmp = *this;
            --current_;
            return tmp;
        }

        constexpr reverse_iterator &operator--()
        {
            ++current_;
            return *this;
        }

        constexpr reverse_iterator operator--(int)
        {
            reverse_iterator tmp = *this;
            ++current_;
            return tmp;
        }

        constexpr reverse_iterator &operator+=(ptrdiff_t offset)
        {
            current_ -= offset;
            return *this;
        }

        constexpr reverse_iterator &operator-=(ptrdiff_t offset)
        {
            current_ += offset;
            return *this;
        }

        [[nodiscard]] constexpr reverse_iterator operator+(ptrdiff_t offset) const
        {
            return reverse_iterator(current_ - offset);
        }

        [[nodiscard]] constexpr reverse_iterator operator-(ptrdiff_t offset) const
        {
            return reverse_iterator(current_ + offset);
        }

        [[nodiscard]] constexpr decltype(auto) operator[](ptrdiff_t offset) const
        {
            return *(*this + offset);
        }

        [[nodiscard]] constexpr bool operator==(const reverse_iterator &other) const
        {
            return current_ == other.current_;
        }

        [[nodiscard]] constexpr bool operator!=(const reverse_iterator &other) const
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const reverse_iterator &other) const
        {
            // Reversed: a "later" reverse_iterator has a smaller underlying position.
            return other.current_ < current_;
        }

        [[nodiscard]] constexpr bool operator>(const reverse_iterator &other) const
        {
            return other < *this;
        }

        [[nodiscard]] constexpr bool operator<=(const reverse_iterator &other) const
        {
            return !(other < *this);
        }

        [[nodiscard]] constexpr bool operator>=(const reverse_iterator &other) const
        {
            return !(*this < other);
        }

    private:
        Iterator current_{};
    };

    template <class Iterator>
    [[nodiscard]] constexpr ptrdiff_t operator-(const reverse_iterator<Iterator> &lhs,
                                                const reverse_iterator<Iterator> &rhs)
    {
        // Reversed order vs a forward iterator subtraction.
        return rhs.base() - lhs.base();
    }

    template <class Iterator>
    [[nodiscard]] constexpr reverse_iterator<Iterator> operator+(ptrdiff_t offset,
                                                                const reverse_iterator<Iterator> &it)
    {
        return it + offset;
    }

    template <class Iterator>
    [[nodiscard]] constexpr reverse_iterator<Iterator> make_reverse_iterator(Iterator it)
    {
        return reverse_iterator<Iterator>(it);
    }
}