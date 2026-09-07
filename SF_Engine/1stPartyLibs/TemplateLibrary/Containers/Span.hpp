#pragma once
#include "../TypeTraits.hpp"
#include "../Types.hpp"

namespace SFTL
{
    template<typename T>
    class span
    {
    public:
        using element_type   = T;
        using value_type     = remove_cv_t<T>;
        using pointer        = T *;
        using const_pointer  = const T *;
        using reference      = T &;
        using iterator       = T *;
        using const_iterator = const T *;

        constexpr span() noexcept = default;

        constexpr span(T *data, size_type count) noexcept : data_(data), size_(count) {}

        constexpr span(T *first, T *last) noexcept : data_(first), size_(static_cast<size_type>(last - first)) {}

        template<size_type N>
        explicit constexpr span(T (&arr)[N]) noexcept : data_(arr), size_(N)
        {
        }

        // Converting ctor: span<T> -> span<const T>, or any U* -> T* that's
        // already implicitly convertible (mirrors std::span's array-conversion rule
        // without dragging in the array-of-unknown-bound trick).
        template<typename U, typename = enable_if_t<is_convertible_v<U *, T *>>>
        explicit constexpr span(const span<U> &other) noexcept : data_(other.Data()), size_(other.Size())
        {
        }

        [[nodiscard]] constexpr T *Data() const noexcept { return data_; }
        [[nodiscard]] constexpr size_type Size() const noexcept { return size_; }
        [[nodiscard]] constexpr size_type SizeBytes() const noexcept { return size_ * sizeof(T); }
        [[nodiscard]] constexpr bool Empty() const noexcept { return size_ == 0; }

        constexpr T &operator[](size_type i) const { return data_[i]; }
        [[nodiscard]] constexpr T &Front() const { return data_[0]; }
        [[nodiscard]] constexpr T &Back() const { return data_[size_ - 1]; }

        [[nodiscard]] constexpr iterator begin() const noexcept { return data_; }
        [[nodiscard]] constexpr iterator end() const noexcept { return data_ + size_; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return data_; }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return data_ + size_; }

        [[nodiscard]] constexpr span First(size_type count) const noexcept { return span(data_, count); }
        [[nodiscard]] constexpr span Last(size_type count) const noexcept
        {
            return span(data_ + (size_ - count), count);
        }
        [[nodiscard]] constexpr span Subspan(size_type offset, size_type count = npos) const noexcept
        {
            size_type n = (count == npos) ? (size_ - offset) : count;
            return span(data_ + offset, n);
        }

        [[nodiscard]] constexpr span<const byte> AsBytes() const noexcept
        {
            return span<const byte>(reinterpret_cast<const byte *>(data_), SizeBytes());
        }

        template<typename U = T, typename = enable_if_t<!is_const_v<U>>>
        [[nodiscard]] constexpr span<byte> AsWritableBytes() const noexcept
        {
            return span<byte>(reinterpret_cast<byte *>(data_), SizeBytes());
        }

        // lowercase aliases, for anything expecting std::span's naming
        [[nodiscard]] constexpr T *data() const noexcept { return data_; }
        [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

        static constexpr size_type npos = static_cast<size_type>(-1);

    private:
        T *data_        = nullptr;
        size_type size_ = 0;
    };

    template<typename T, size_type N>
    span(T (&)[N]) -> span<T>;

    template<typename T>
    span(T *, size_type) -> span<T>;

    using ByteSpan      = span<byte>;
    using ConstByteSpan = span<const byte>;
} // namespace SFTL
