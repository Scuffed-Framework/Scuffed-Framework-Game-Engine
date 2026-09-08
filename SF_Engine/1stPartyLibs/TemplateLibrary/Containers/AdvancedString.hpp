#pragma once
#include "../Algorithm.hpp"
#include "../Allocator.hpp"
#include "../Compare.hpp"
#include "Span.hpp"

namespace SFTL
{
    namespace Detail
    {
        template<typename It, typename Val>
        constexpr void FillN(It first, size_type n, const Val &v)
        {
            for (size_type i = 0; i < n; ++i, ++first)
                *first = v;
        }

        template<typename It, typename Val>
        constexpr void Fill(It first, It last, const Val &v)
        {
            for (; first != last; ++first)
                *first = v;
        }

        template<typename A, typename B>
        constexpr B MoveBackward(A first, A last, B last2)
        {
            while (first != last)
                *(--last2) = ::SFTL::move(*(--last));
            return last2;
        }

        constexpr size_type HashSpan(const void *ptr, size_type byteCount)
        {
            const auto *bytes = static_cast<const unsigned char *>(ptr);
            size_type hash    = 14695981039346656037ull;
            for (size_type i = 0; i < byteCount; ++i)
            {
                constexpr size_type prime = 1099511628211ull;
                hash ^= bytes[i];
                hash *= prime;
            }
            return hash;
        }
    } // namespace Detail

    template<typename T>
    class AdvancedStringView
    {
        const T *data_  = nullptr;
        size_type size_ = 0;

    public:
        constexpr AdvancedStringView() = default;
        constexpr AdvancedStringView(const T *d, size_type s) : data_(d), size_(s) {}

        [[nodiscard]] constexpr const T *Data() const { return data_; }
        [[nodiscard]] constexpr size_type Size() const { return size_; }
        [[nodiscard]] constexpr bool Empty() const { return size_ == 0; }
        [[nodiscard]] constexpr const T *begin() const { return data_; }
        [[nodiscard]] constexpr const T *end() const { return data_ + size_; }
        constexpr const T &operator[](size_type i) const { return data_[i]; }

        constexpr operator ::SFTL::span<const T>() const { return span<const T>(data_, size_); }

        constexpr bool operator==(const AdvancedStringView &rhs) const
        {
            return size_ == rhs.size_ && (data_ == rhs.data_ || equal(begin(), end(), rhs.begin()));
        }

        constexpr bool operator==(const T *rhs) const
        {
            size_type len = strlen(rhs);
            return size_ == len && equal(begin(), end(), rhs);
        }

        constexpr auto operator<=>(const AdvancedStringView &rhs) const
        {
            return ::SFTL::lexicographical_compare_three_way(begin(), end(), rhs.begin(), rhs.end());
        }
    };

    template<typename T, typename Allocator = allocator<T>>
    class AdvancedString
    {
        static_assert(is_trivial_v<T>, "AdvancedString requires a trivial character type");

    private:
        static constexpr size_type kInlineBytes    = sizeof(T *) + sizeof(size_type);
        static constexpr size_type kInlineCapacity = (kInlineBytes / sizeof(T)) > 1 ? (kInlineBytes / sizeof(T)) - 1
                                                                                    : 1;

        union Storage
        {
            T inlineBuf[kInlineCapacity + 1];
            T *heapBuf;
            constexpr Storage() : inlineBuf{} {}
            constexpr ~Storage() {}
        };

        Storage storage_;
        size_type size_     = 0;
        size_type capacity_ = kInlineCapacity;
        Allocator alloc_;

        [[nodiscard]] constexpr bool IsHeap() const { return capacity_ > kInlineCapacity; }
        constexpr T *Ptr() { return IsHeap() ? storage_.heapBuf : storage_.inlineBuf; }
        [[nodiscard]] constexpr const T *Ptr() const { return IsHeap() ? storage_.heapBuf : storage_.inlineBuf; }

        constexpr void DestroyHeap()
        {
            if (IsHeap())
                ::SFTL::allocator_traits<Allocator>::deallocate(alloc_, storage_.heapBuf, capacity_ + 1);
        }

        constexpr void GrowPreserving(size_type newCapacity)
        {
            if (newCapacity <= capacity_)
                return;

            size_type grown = capacity_ + capacity_ / 2;
            if (grown < newCapacity)
                grown = newCapacity;

            T *newBuf = ::SFTL::allocator_traits<Allocator>::allocate(alloc_, grown + 1);
            copy(Ptr(), Ptr() + size_, newBuf);
            DestroyHeap();
            storage_.heapBuf = newBuf;
            capacity_        = grown;
        }

        constexpr void AssignRaw(const T *src, size_type count)
        {
            if (count > capacity_)
            {
                size_type grown = capacity_ + capacity_ / 2;
                if (grown < count)
                    grown = count;

                T *newBuf = ::SFTL::allocator_traits<Allocator>::allocate(alloc_, grown + 1);
                DestroyHeap();
                storage_.heapBuf = newBuf;
                capacity_        = grown;
            }
            copy(src, src + count, Ptr());
            size_        = count;
            Ptr()[size_] = T{};
        }

        constexpr void MoveFrom(AdvancedString &other) noexcept
        {
            if (other.IsHeap())
            {
                storage_.heapBuf = other.storage_.heapBuf;
                capacity_        = other.capacity_;
            } else
            {
                copy(other.storage_.inlineBuf, other.storage_.inlineBuf + other.size_ + 1, storage_.inlineBuf);
                capacity_ = kInlineCapacity;
            }
            size_                       = other.size_;
            other.storage_.inlineBuf[0] = T{};
            other.size_                 = 0;
            other.capacity_             = kInlineCapacity;
        }

        template<typename U>
        static constexpr void AppendOne(AdvancedString &out, const U &value)
        {
            using D = decay_t<U>;
            if constexpr (is_same_v<D, bool>)
            {
                out.append(value ? "true" : "false", value ? 4 : 5);
            } else if constexpr (is_integral_v<D>)
            {
                AppendInteger(out, value);
            } else if constexpr (is_floating_point_v<D>)
            {
                AppendFloat(out, static_cast<double>(value));
            } else if constexpr (is_pointer_v<D> && is_same_v<remove_cv_t<remove_pointer_t<D>>, T>)
            {
                out.append(value, strlen(value));
            } else
            {
                out.append(value);
            }
        }

        template<typename Int>
        static constexpr void AppendInteger(AdvancedString &out, Int value)
        {
            char buf[24];
            size_type len = 0;
            bool neg      = false;
            unsigned long long mag;
            if constexpr (is_signed_v<Int>)
            {
                neg = value < 0;
                mag = neg ? (0ULL - static_cast<unsigned long long>(value)) : static_cast<unsigned long long>(value);
            } else
            {
                mag = static_cast<unsigned long long>(value);
            }
            if (mag == 0)
                buf[len++] = '0';
            while (mag > 0)
            {
                buf[len++] = static_cast<char>('0' + (mag % 10));
                mag /= 10;
            }
            if (neg)
                buf[len++] = '-';
            for (size_type i = len; i > 0; --i)
                out.push_back(static_cast<T>(buf[i - 1]));
        }

        static constexpr void AppendFloat(AdvancedString &out, double value)
        {
            if (value < 0)
            {
                out.push_back(static_cast<T>('-'));
                value = -value;
            }
            auto intPart = static_cast<unsigned long long>(value);
            double frac  = value - static_cast<double>(intPart);
            AppendInteger(out, intPart);
            out.push_back(static_cast<T>('.'));
            for (int i = 0; i < 6; ++i)             // fixed 6-digit precision, this is not a
            {                                       // shortest-round-trip float formatter (that's
                frac *= 10.0;                       // what <charconv> is for); it's a plain fixed
                int digit = static_cast<int>(frac); // decimal, which is all Format() needs.
                out.push_back(static_cast<T>('0' + digit));
                frac -= digit;
            }
        }

        // Appends literal text up to the next unescaped "{}" placeholder,
        // resolving "{{" -> "{" and "}}" -> "}". Returns true if a placeholder
        // was consumed (cursor left just past it); false if the string ended
        // first (cursor left at the terminating null).
        static constexpr bool ConsumeUntilPlaceholder(AdvancedString &out, const T *&cursor)
        {
            while (*cursor != T{})
            {
                T c = *cursor;
                if (c == static_cast<T>('{'))
                {
                    if (cursor[1] == static_cast<T>('{'))
                    {
                        out.push_back(c);
                        cursor += 2;
                        continue;
                    }
                    if (cursor[1] == static_cast<T>('}'))
                    {
                        cursor += 2;
                        return true;
                    }
                    out.push_back(c);
                    ++cursor;
                    continue;
                }
                if (c == static_cast<T>('}') && cursor[1] == static_cast<T>('}'))
                {
                    out.push_back(c);
                    cursor += 2;
                    continue;
                }
                out.push_back(c);
                ++cursor;
            }
            return false;
        }

        static constexpr void FormatImpl(AdvancedString &out, const T *&cursor)
        {
            ConsumeUntilPlaceholder(out, cursor);
        }

        template<typename First, typename... Rest>
        static constexpr void FormatImpl(AdvancedString &out, const T *&cursor, const First &first, const Rest &...rest)
        {
            if (ConsumeUntilPlaceholder(out, cursor))
                AppendOne(out, first);
            FormatImpl(out, cursor, rest...);
        }

    public:
        using value_type             = T;
        using allocator_type         = Allocator;
        using difference_type        = ptrdiff_t;
        using reference              = T &;
        using const_reference        = const T &;
        using pointer                = typename ::SFTL::allocator_traits<Allocator>::pointer;
        using const_pointer          = typename ::SFTL::allocator_traits<Allocator>::const_pointer;
        using iterator               = T *;
        using const_iterator         = const T *;
        using reverse_iterator       = ::SFTL::reverse_iterator<iterator>;
        using const_reverse_iterator = ::SFTL::reverse_iterator<const_iterator>;

        static constexpr size_type npos = static_cast<size_type>(-1);

        constexpr AdvancedString() = default;

        explicit constexpr AdvancedString(const Allocator &alloc) : alloc_(alloc) {}

        constexpr AdvancedString(size_type count, T ch, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            GrowPreserving(count);
            Detail::FillN(Ptr(), count, ch);
            size_        = count;
            Ptr()[size_] = T{};
        }

        explicit constexpr AdvancedString(span<const T> src, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            AssignRaw(src.Data(), src.Size());
        }

        constexpr AdvancedString(const T *ptr, size_type count, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            AssignRaw(ptr, count);
        }

        constexpr AdvancedString(const T *cstr, const Allocator &alloc = Allocator()) :
            AdvancedString(cstr, strlen(cstr), alloc)
        {
        }

        constexpr AdvancedString(const AdvancedStringView<T> &view, const Allocator &alloc = Allocator()) :
            AdvancedString(view.Data(), view.Size(), alloc)
        {
        }

        template<class InputIt>
        constexpr AdvancedString(InputIt first, InputIt last, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        constexpr AdvancedString(const AdvancedString &other) : alloc_(other.alloc_)
        {
            AssignRaw(other.Ptr(), other.size_);
        }

        constexpr AdvancedString(const AdvancedString &other, size_type pos, size_type count = npos,
                                 const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            pos   = min(pos, other.size_);
            count = min(count, other.size_ - pos);
            AssignRaw(other.Ptr() + pos, count);
        }

        constexpr AdvancedString(AdvancedString &&other) noexcept : alloc_(move(other.alloc_)) { MoveFrom(other); }

        explicit constexpr AdvancedString(initializer_list<T> ilist, const Allocator &alloc = Allocator()) :
            alloc_(alloc)
        {
            AssignRaw(ilist.begin(), ilist.size());
        }

        constexpr AdvancedString &operator=(const AdvancedString &other)
        {
            if (this != &other)
            {
                alloc_ = other.alloc_;
                AssignRaw(other.Ptr(), other.size_);
            }
            return *this;
        }

        constexpr AdvancedString &operator=(AdvancedString &&other) noexcept
        {
            if (this != &other)
            {
                DestroyHeap();
                alloc_ = move(other.alloc_);
                MoveFrom(other);
            }
            return *this;
        }

        constexpr AdvancedString &operator=(const T *cstr)
        {
            AssignRaw(cstr, strlen(cstr));
            return *this;
        }
        constexpr AdvancedString &operator=(T ch)
        {
            AssignRaw(&ch, 1);
            return *this;
        }
        constexpr AdvancedString &operator+=(T *cstr)
        {
            Append(cstr);
            return *this;
        }

        constexpr ~AdvancedString() { DestroyHeap(); }

        constexpr reference at(size_type i)
        {
            if (i >= size_)
                return nullptr;
            return Ptr()[i];
        }
        [[nodiscard]] constexpr const_reference at(size_type i) const
        {
            if (i >= size_)
                return nullptr;
            return Ptr()[i];
        }

        constexpr reference operator[](size_type i) { return Ptr()[i]; }
        constexpr const_reference operator[](size_type i) const { return Ptr()[i]; }

        constexpr reference front() { return Ptr()[0]; }
        [[nodiscard]] constexpr const_reference front() const { return Ptr()[0]; }
        constexpr reference back() { return Ptr()[size_ - 1]; }
        [[nodiscard]] constexpr const_reference back() const { return Ptr()[size_ - 1]; }

        [[nodiscard]] constexpr const T *data() const noexcept { return Ptr(); }
        constexpr T *data() noexcept { return Ptr(); }
        [[nodiscard]] constexpr const T *c_str() const noexcept { return Ptr(); }

        [[nodiscard]] constexpr span<const T> AsSpan() const { return span<const T>(Ptr(), size_); }
        [[nodiscard]] constexpr AdvancedStringView<T> View() const { return {Ptr(), size_}; }
        constexpr explicit operator AdvancedStringView<T>() const { return View(); }
        constexpr explicit operator span<const T>() const { return AsSpan(); }

        constexpr iterator begin() noexcept { return Ptr(); }
        constexpr iterator end() noexcept { return Ptr() + size_; }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return Ptr(); }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return Ptr() + size_; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return Ptr(); }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return Ptr() + size_; }

        constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
        [[nodiscard]] constexpr size_type length() const noexcept { return size_; }
        [[nodiscard]] constexpr size_type capacity() const noexcept { return capacity_; }
        [[nodiscard]] constexpr size_type max_size() const noexcept
        {
            return static_cast<size_type>(-1) / sizeof(T) - 1;
        }

        constexpr void reserve(size_type n) { GrowPreserving(n); }

        constexpr void shrink_to_fit()
        {
            if (!IsHeap() || size_ == capacity_)
                return;
            if (size_ <= kInlineCapacity)
            {
                T *heap     = storage_.heapBuf;
                size_type n = size_;
                copy(heap, heap + n, storage_.inlineBuf);
                ::SFTL::allocator_traits<Allocator>::deallocate(alloc_, heap, capacity_ + 1);
                size_        = n;
                capacity_    = kInlineCapacity;
                Ptr()[size_] = T{};
                return;
            }
            T *newBuf = ::SFTL::allocator_traits<Allocator>::allocate(alloc_, size_ + 1);
            copy(storage_.heapBuf, storage_.heapBuf + size_, newBuf);
            DestroyHeap();
            storage_.heapBuf = newBuf;
            capacity_        = size_;
            Ptr()[size_]     = T{};
        }

        constexpr void clear() noexcept
        {
            size_    = 0;
            Ptr()[0] = T{};
        }

        constexpr void push_back(T ch)
        {
            GrowPreserving(size_ + 1);
            Ptr()[size_++] = ch;
            Ptr()[size_]   = T{};
        }

        constexpr void pop_back()
        {
            if (size_ > 0)
            {
                --size_;
                Ptr()[size_] = T{};
            }
        }

        constexpr void resize(size_type count, T ch = T{})
        {
            if (count > size_)
            {
                GrowPreserving(count);
                Detail::Fill(Ptr() + size_, Ptr() + count, ch);
            }
            size_        = count;
            Ptr()[size_] = T{};
        }

        constexpr AdvancedString &append(const T *src, size_type count)
        {
            GrowPreserving(size_ + count);
            copy(src, src + count, Ptr() + size_);
            size_ += count;
            Ptr()[size_] = T{};
            return *this;
        }
        constexpr AdvancedString &append(const AdvancedStringView<T> &sv) { return append(sv.Data(), sv.Size()); }
        constexpr AdvancedString &append(const AdvancedString &other) { return append(other.Ptr(), other.size_); }
        constexpr AdvancedString &append(size_type count, T ch)
        {
            GrowPreserving(size_ + count);
            Detail::FillN(Ptr() + size_, count, ch);
            size_ += count;
            Ptr()[size_] = T{};
            return *this;
        }

        constexpr AdvancedString &operator+=(const AdvancedString &other) { return append(other); }
        constexpr AdvancedString &operator+=(const AdvancedStringView<T> &sv) { return append(sv); }
        constexpr AdvancedString &operator+=(T ch)
        {
            push_back(ch);
            return *this;
        }
        constexpr AdvancedString &operator+=(const T *cstr) { return append(cstr, strlen(cstr)); }

        constexpr iterator insert(const_iterator pos, size_type count, T ch)
        {
            auto idx = static_cast<size_type>(pos - begin());
            GrowPreserving(size_ + count);
            T *p = Ptr();
            Detail::MoveBackward(p + idx, p + size_, p + size_ + count);
            Detail::FillN(p + idx, count, ch);
            size_ += count;
            p[size_] = T{};
            return p + idx;
        }

        constexpr AdvancedString &insert(size_type index, const T *src, size_type count)
        {
            index = min(index, size_);
            GrowPreserving(size_ + count);
            T *p = Ptr();
            Detail::MoveBackward(p + index, p + size_, p + size_ + count);
            copy(src, src + count, p + index);
            size_ += count;
            p[size_] = T{};
            return *this;
        }
        constexpr AdvancedString &insert(size_type index, const AdvancedStringView<T> &sv)
        {
            return insert(index, sv.Data(), sv.Size());
        }

        constexpr iterator erase(const_iterator first, const_iterator last)
        {
            T *p       = Ptr();
            auto start = static_cast<size_type>(first - begin());
            auto count = static_cast<size_type>(last - first);
            move(p + start + count, p + size_, p + start);
            size_ -= count;
            p[size_] = T{};
            return p + start;
        }
        constexpr iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
        constexpr AdvancedString &erase(size_type index = 0, size_type count = npos)
        {
            index = min(index, size_);
            count = min(count, size_ - index);
            erase(begin() + index, begin() + index + count);
            return *this;
        }

        constexpr void swap(AdvancedString &other) noexcept
        {
            AdvancedString tmp(move(other));
            other = move(*this);
            *this = move(tmp);
        }

        [[nodiscard]] constexpr AdvancedString substr(size_type pos = 0, size_type count = npos) const
        {
            pos   = min(pos, size_);
            count = min(count, size_ - pos);
            return AdvancedString(Ptr() + pos, count, alloc_);
        }

        // legacy view-returning accessor, prefer substr() for std::string parity
        [[nodiscard]] constexpr AdvancedStringView<T> SubstrView(size_type offset, size_type count = npos) const
        {
            offset = min(offset, size_);
            count  = min(count, size_ - offset);
            return AdvancedStringView<T>(Ptr() + offset, count);
        }

        [[nodiscard]] constexpr int compare(const AdvancedString &rhs) const noexcept
        {
            size_type n = min(size_, rhs.size_);
            int r       = n ? compare(Ptr(), rhs.Ptr(), n) : 0;
            if (r != 0)
                return r;
            if (size_ < rhs.size_)
                return -1;
            if (size_ > rhs.size_)
                return 1;
            return 0;
        }
        [[nodiscard]] constexpr int compare(const AdvancedStringView<T> &rhs) const noexcept
        {
            size_type n = min(size_, rhs.Size());
            int r       = n ? compare(Ptr(), rhs.Data(), n) : 0;
            if (r != 0)
                return r;
            if (size_ < rhs.Size())
                return -1;
            if (size_ > rhs.Size())
                return 1;
            return 0;
        }

        [[nodiscard]] constexpr size_type find(T ch, size_type from = 0) const noexcept
        {
            for (size_type i = from; i < size_; ++i)
                if (Ptr()[i] == ch)
                    return i;
            return npos;
        }

        [[nodiscard]] constexpr size_type find(const AdvancedStringView<T> &needle, size_type from = 0) const noexcept
        {
            if (needle.Empty())
                return from <= size_ ? from : npos;
            if (needle.Size() > size_)
                return npos;
            for (size_type i = from; i + needle.Size() <= size_; ++i)
                if (equal(needle.begin(), needle.end(), Ptr() + i))
                    return i;
            return npos;
        }

        [[nodiscard]] constexpr size_type rfind(T ch, size_type from = npos) const noexcept
        {
            if (size_ == 0)
                return npos;
            size_type i = min(from, size_ - 1);
            for (;; --i)
            {
                if (Ptr()[i] == ch)
                    return i;
                if (i == 0)
                    break;
            }
            return npos;
        }

        [[nodiscard]] constexpr size_type find_first_of(T ch, size_type from = 0) const noexcept
        {
            return find(ch, from);
        }
        [[nodiscard]] constexpr size_type find_first_not_of(T ch, size_type from = 0) const noexcept
        {
            for (size_type i = from; i < size_; ++i)
                if (Ptr()[i] != ch)
                    return i;
            return npos;
        }

        [[nodiscard]] constexpr bool starts_with(const AdvancedStringView<T> &sv) const noexcept
        {
            return sv.Size() <= size_ && equal(sv.begin(), sv.end(), Ptr());
        }
        [[nodiscard]] constexpr bool ends_with(const AdvancedStringView<T> &sv) const noexcept
        {
            return sv.Size() <= size_ && equal(sv.begin(), sv.end(), Ptr() + size_ - sv.Size());
        }
        [[nodiscard]] constexpr bool contains(const AdvancedStringView<T> &sv) const noexcept
        {
            return find(sv) != npos;
        }
        [[nodiscard]] constexpr bool contains(T ch) const noexcept { return find(ch) != npos; }

        [[nodiscard]] constexpr bool IsSmall() const { return !IsHeap(); }

        [[nodiscard]] constexpr AdvancedString Trim() const
        {
            size_type start = 0;
            size_type end_  = size_;
            const T *p      = Ptr();

            while (start < end_ && is_space(p[start]))
                ++start;
            while (end_ > start && is_space(p[end_ - 1]))
                --end_;

            return AdvancedString(p + start, end_ - start, alloc_);
        }

        // Type-safe "{}"-placeholder formatter. "{{" / "}}" escape to literal
        // braces. Unlike printf/vsnprintf there is no format-string/argument
        // mismatch to get wrong - argument types are deduced, not declared.
        //   AdvancedString::Format("{} of {} ({}%)", 3, 4, 75.0);
        //   -> "3 of 4 (75.000000%)"
        template<typename... Args>
        static constexpr AdvancedString Format(const T *fmt, const Args &...args)
        {
            AdvancedString result;
            const T *cursor = fmt;
            FormatImpl(result, cursor, args...);
            return result;
        }

        constexpr bool operator==(const AdvancedString &rhs) const { return View() == rhs.View(); }
        constexpr auto operator<=>(const AdvancedString &rhs) const { return View() <=> rhs.View(); }
        constexpr bool operator==(const AdvancedStringView<T> &rhs) const { return View() == rhs; }

        friend constexpr AdvancedString operator+(const AdvancedString &lhs, const AdvancedString &rhs)
        {
            AdvancedString result(lhs);
            result.append(rhs);
            return result;
        }
        friend constexpr AdvancedString operator+(AdvancedString &&lhs, const AdvancedString &rhs)
        {
            lhs.append(rhs);
            return move(lhs);
        }

        constexpr AdvancedString &operator<<(const AdvancedString &s)
        {
            append(s);
            return *this;
        }

        constexpr AdvancedString &operator>>(AdvancedString &target) noexcept
        {
            auto view       = this->View();
            size_type start = 0;
            while (start < view.size() && is_space(static_cast<unsigned char>(view[start])))
            {
                start++;
            }
            size_type end = start;
            while (end < view.size() && !is_space(static_cast<unsigned char>(view[end])))
            {
                end++;
            }

            if (start < view.size())
            {
                target = AdvancedString(view.substr(start, end - start));
                *this  = AdvancedString(view.substr(end));
            } else
            {
                target.clear(); // Nothing left to extract
            }

            return *this;
        }

        constexpr AdvancedString *IfContains(const AdvancedString &s) noexcept
        {
            return contains(s.View()) ? this : nullptr;
        }

        [[nodiscard]] constexpr const T *Data() const { return Ptr(); }
        [[nodiscard]] constexpr const T *CStr() const { return Ptr(); }
        [[nodiscard]] constexpr size_type Size() const { return size_; }
        [[nodiscard]] constexpr size_type Length() const { return size_; }
        [[nodiscard]] constexpr size_type Capacity() const { return capacity_; }
        [[nodiscard]] constexpr bool Empty() const { return size_ == 0; }

        constexpr void Clear() { clear(); }
        constexpr void Reserve(size_type n) { reserve(n); }
        constexpr AdvancedString &Append(const T *src, size_type count) { return append(src, count); }
        constexpr AdvancedString &Append(const AdvancedStringView<T> &sv) { return append(sv); }
        constexpr AdvancedString &Append(const AdvancedString &other) { return append(other); }
        [[nodiscard]] constexpr AdvancedStringView<T> Substr(size_type offset, size_type count = npos) const
        {
            return SubstrView(offset, count);
        }
        [[nodiscard]] constexpr size_type Find(const T &c, size_type from = 0) const { return find(c, from); }
    };


#define DEFINE_STRING_LITERAL_OPERATOR(StringType, Suffix)                                                             \
    constexpr StringType operator""##Suffix(const char *str, decltype(sizeof(0)) len) noexcept                         \
    {                                                                                                                  \
        return StringType(AdvancedStringView<char>(str, len));                                                         \
    }


    template<typename T, typename Allocator = allocator<T>>
    AdvancedString<T, Allocator> MakeAdvancedString(span<const T> data)
    {
        return AdvancedString<T, Allocator>(data);
    }

    template<typename T, typename Allocator = allocator<T>>
    AdvancedString<T, Allocator> Str()
    {
        return AdvancedString<T, Allocator>();
    }


    template<typename Key>
    struct hash;

    template<typename T, typename Allocator>
    struct hash<SFTL::AdvancedString<T, Allocator>>
    {
        size_t operator()(const SFTL::AdvancedString<T, Allocator> &s) const noexcept
        {
            return SFTL::Detail::HashSpan(s.Data(), s.Size() * sizeof(T));
        }
    };

    template<typename T>
    struct hash<SFTL::AdvancedStringView<T>>
    {
        size_t operator()(const SFTL::AdvancedStringView<T> &s) const noexcept
        {
            return SFTL::Detail::HashSpan(s.Data(), s.Size() * sizeof(T));
        }
    };
} // namespace SFTL
