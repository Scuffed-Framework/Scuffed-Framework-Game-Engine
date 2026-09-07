#pragma once
#include "Types.hpp"
namespace SFTL
{
    struct input_iterator_tag
    {
    };

    struct output_iterator_tag
    {
    };

    struct forward_iterator_tag : public input_iterator_tag
    {
    };

    struct bidirectional_iterator_tag : public forward_iterator_tag
    {
    };

    struct random_access_iterator_tag : public bidirectional_iterator_tag
    {
    };

    struct contiguous_iterator_tag : public random_access_iterator_tag
    {
    };

    template<typename Iterator, typename = void>
    struct iterator_traits
    {
    };

    // Specialization for iterators that define standard member types
    template<typename Iterator>
    struct iterator_traits<Iterator, void_t<typename Iterator::iterator_category, typename Iterator::value_type,
                                            typename Iterator::difference_type, typename Iterator::pointer,
                                            typename Iterator::reference>>
    {
        using iterator_category = typename Iterator::iterator_category;
        using value_type        = typename Iterator::value_type;
        using difference_type   = typename Iterator::difference_type;
        using pointer           = typename Iterator::pointer;
        using reference         = typename Iterator::reference;
    };

    // Specialization for mutable native pointers (T*) -> Contiguous Iterator
    template<typename T>
    struct iterator_traits<T *>
    {
        using iterator_category = contiguous_iterator_tag;
        using value_type        = remove_cv_t<T>;
        using difference_type   = ptrdiff_t;
        using pointer           = T *;
        using reference         = T &;
    };

    // Specialization for const native pointers (const T*) -> Contiguous Iterator
    template<typename T>
    struct iterator_traits<const T *>
    {
        using iterator_category = contiguous_iterator_tag;
        using value_type        = remove_cv_t<T>;
        using difference_type   = ptrdiff_t;
        using pointer           = const T *;
        using reference         = const T &;
    };

    template<typename Iterator>
    using iterator_category_t = typename iterator_traits<Iterator>::iterator_category;

    template<typename InputIterator>
    using RequireInputIter = enable_if_t<is_convertible<iterator_category_t<InputIterator>, input_iterator_tag>::value>;

    template<class Iterator>
    class reverse_iterator
    {
    public:
        using iterator_type = Iterator;

        using iterator_category = typename iterator_traits<Iterator>::iterator_category;
        using value_type        = typename iterator_traits<Iterator>::value_type;
        using difference_type   = typename iterator_traits<Iterator>::difference_type;
        using pointer           = typename iterator_traits<Iterator>::pointer;
        using reference         = typename iterator_traits<Iterator>::reference;

        constexpr reverse_iterator() = default;

        constexpr explicit reverse_iterator(Iterator it) : current_(it) {}

        // Allows converting a reverse_iterator over a convertible base iterator
        // (e.g. reverse_iterator<T*> -> reverse_iterator<const T*>).
        template<class Other>
        constexpr reverse_iterator(const reverse_iterator<Other> &other) : current_(other.base())
        {
        }

        [[nodiscard]] constexpr Iterator base() const { return current_; }

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

        [[nodiscard]] constexpr decltype(auto) operator[](ptrdiff_t offset) const { return *(*this + offset); }

        [[nodiscard]] constexpr bool operator==(const reverse_iterator &other) const
        {
            return current_ == other.current_;
        }

        [[nodiscard]] constexpr bool operator!=(const reverse_iterator &other) const { return !(*this == other); }

        [[nodiscard]] constexpr bool operator<(const reverse_iterator &other) const
        {
            // Reversed: a "later" reverse_iterator has a smaller underlying position.
            return other.current_ < current_;
        }

        [[nodiscard]] constexpr bool operator>(const reverse_iterator &other) const { return other < *this; }

        [[nodiscard]] constexpr bool operator<=(const reverse_iterator &other) const { return !(other < *this); }

        [[nodiscard]] constexpr bool operator>=(const reverse_iterator &other) const { return !(*this < other); }

    private:
        Iterator current_{};
    };

    template<class Iterator>
    [[nodiscard]] constexpr ptrdiff_t operator-(const reverse_iterator<Iterator> &lhs,
                                                const reverse_iterator<Iterator> &rhs)
    {
        // Reversed order vs a forward iterator subtraction.
        return rhs.base() - lhs.base();
    }

    template<class Iterator>
    [[nodiscard]] constexpr reverse_iterator<Iterator> operator+(ptrdiff_t offset, const reverse_iterator<Iterator> &it)
    {
        return it + offset;
    }

    template<class Iterator>
    [[nodiscard]] constexpr reverse_iterator<Iterator> make_reverse_iterator(Iterator it)
    {
        return reverse_iterator<Iterator>(it);
    }

    template<typename T>
    class contiguous_iterator
    {
    public:
        using iterator_concept  = contiguous_iterator_tag;
        using iterator_category = random_access_iterator_tag;
        using value_type        = remove_cv_t<T>;
        using difference_type   = ptrdiff_t;
        using pointer           = T *;
        using reference         = T &;

        constexpr contiguous_iterator() noexcept = default;
        constexpr explicit contiguous_iterator(pointer ptr) noexcept : ptr_(ptr) {}

        // Allow conversion from non-const to const iterator
        template<typename U,
                 enable_if_t<is_same_v<remove_cv_t<U>, remove_cv_t<T>> && is_const_v<T> && !is_const_v<U>, int> = 0>
        constexpr contiguous_iterator(const contiguous_iterator<U> &other) noexcept : ptr_(other.base())
        {
        }

        [[nodiscard]] constexpr pointer base() const noexcept { return ptr_; }

        [[nodiscard]] constexpr reference operator*() const noexcept { return *ptr_; }
        [[nodiscard]] constexpr pointer operator->() const noexcept { return ptr_; }

        constexpr contiguous_iterator &operator++() noexcept
        {
            ++ptr_;
            return *this;
        }

        constexpr contiguous_iterator operator++(int) noexcept
        {
            contiguous_iterator tmp = *this;
            ++ptr_;
            return tmp;
        }

        constexpr contiguous_iterator &operator--() noexcept
        {
            --ptr_;
            return *this;
        }

        constexpr contiguous_iterator operator--(int) noexcept
        {
            contiguous_iterator tmp = *this;
            --ptr_;
            return tmp;
        }

        constexpr contiguous_iterator &operator+=(difference_type n) noexcept
        {
            ptr_ += n;
            return *this;
        }

        constexpr contiguous_iterator &operator-=(difference_type n) noexcept
        {
            ptr_ -= n;
            return *this;
        }

        [[nodiscard]] constexpr contiguous_iterator operator+(difference_type n) const noexcept
        {
            return contiguous_iterator(ptr_ + n);
        }

        [[nodiscard]] constexpr contiguous_iterator operator-(difference_type n) const noexcept
        {
            return contiguous_iterator(ptr_ - n);
        }

        [[nodiscard]] constexpr difference_type operator-(const contiguous_iterator &other) const noexcept
        {
            return ptr_ - other.ptr_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type n) const noexcept { return ptr_[n]; }

        // Comparison operators
        [[nodiscard]] constexpr bool operator==(const contiguous_iterator &other) const noexcept
        {
            return ptr_ == other.ptr_;
        }
        [[nodiscard]] constexpr bool operator!=(const contiguous_iterator &other) const noexcept
        {
            return ptr_ != other.ptr_;
        }
        [[nodiscard]] constexpr bool operator<(const contiguous_iterator &other) const noexcept
        {
            return ptr_ < other.ptr_;
        }
        [[nodiscard]] constexpr bool operator>(const contiguous_iterator &other) const noexcept
        {
            return ptr_ > other.ptr_;
        }
        [[nodiscard]] constexpr bool operator<=(const contiguous_iterator &other) const noexcept
        {
            return ptr_ <= other.ptr_;
        }
        [[nodiscard]] constexpr bool operator>=(const contiguous_iterator &other) const noexcept
        {
            return ptr_ >= other.ptr_;
        }

    private:
        pointer ptr_ = nullptr;
    };

    template<typename T>
    [[nodiscard]] constexpr contiguous_iterator<T> operator+(typename contiguous_iterator<T>::difference_type n,
                                                             const contiguous_iterator<T> &it) noexcept
    {
        return it + n;
    }

    template<class Iterator>
    class move_iterator
    {
    public:
        using iterator_type     = Iterator;
        using iterator_category = typename iterator_traits<Iterator>::iterator_category;
        using value_type        = typename iterator_traits<Iterator>::value_type;
        using difference_type   = typename iterator_traits<Iterator>::difference_type;
        using pointer           = typename iterator_traits<Iterator>::pointer;
        using reference         = value_type &&; // Rvalue reference type for moving

        constexpr move_iterator() noexcept = default;

        constexpr explicit move_iterator(Iterator it) noexcept : current_(it) {}

        template<class U>
        constexpr move_iterator(const move_iterator<U> &other) noexcept : current_(other.base())
        {
        }

        [[nodiscard]] constexpr Iterator base() const noexcept { return current_; }

        [[nodiscard]] constexpr reference operator*() const noexcept { return static_cast<reference>(*current_); }

        [[nodiscard]] constexpr pointer operator->() const noexcept { return current_; }

        constexpr move_iterator &operator++() noexcept
        {
            ++current_;
            return *this;
        }

        constexpr move_iterator operator++(int) noexcept
        {
            move_iterator tmp = *this;
            ++current_;
            return tmp;
        }

        constexpr move_iterator &operator--() noexcept
        {
            --current_;
            return *this;
        }

        constexpr move_iterator operator--(int) noexcept
        {
            move_iterator tmp = *this;
            --current_;
            return tmp;
        }

        constexpr move_iterator &operator+=(difference_type n) noexcept
        {
            current_ += n;
            return *this;
        }

        constexpr move_iterator &operator-=(difference_type n) noexcept
        {
            current_ -= n;
            return *this;
        }

        [[nodiscard]] constexpr move_iterator operator+(difference_type n) const noexcept
        {
            return move_iterator(current_ + n);
        }

        [[nodiscard]] constexpr move_iterator operator-(difference_type n) const noexcept
        {
            return move_iterator(current_ - n);
        }

        [[nodiscard]] constexpr reference operator[](difference_type n) const noexcept
        {
            return static_cast<reference>(current_[n]);
        }

        // Comparison Operators
        [[nodiscard]] constexpr bool operator==(const move_iterator &other) const noexcept
        {
            return current_ == other.current_;
        }

        [[nodiscard]] constexpr bool operator!=(const move_iterator &other) const noexcept
        {
            return current_ != other.current_;
        }

        [[nodiscard]] constexpr bool operator<(const move_iterator &other) const noexcept
        {
            return current_ < other.current_;
        }

        [[nodiscard]] constexpr bool operator>(const move_iterator &other) const noexcept
        {
            return current_ > other.current_;
        }

        [[nodiscard]] constexpr bool operator<=(const move_iterator &other) const noexcept
        {
            return current_ <= other.current_;
        }

        [[nodiscard]] constexpr bool operator>=(const move_iterator &other) const noexcept
        {
            return current_ >= other.current_;
        }

    private:
        Iterator current_{};
    };

    // Non-member arithmetic and helper functions
    template<class Iterator>
    [[nodiscard]] constexpr ptrdiff_t operator-(const move_iterator<Iterator> &lhs,
                                                const move_iterator<Iterator> &rhs) noexcept
    {
        return lhs.base() - rhs.base();
    }

    template<class Iterator>
    [[nodiscard]] constexpr move_iterator<Iterator> operator+(typename move_iterator<Iterator>::difference_type n,
                                                              const move_iterator<Iterator> &it) noexcept
    {
        return it + n;
    }

    // make_move_iterator helper function deduction wrapper
    template<class Iterator>
    [[nodiscard]] constexpr move_iterator<Iterator> make_move_iterator(Iterator it) noexcept
    {
        return move_iterator<Iterator>(it);
    }

} // namespace SFTL
