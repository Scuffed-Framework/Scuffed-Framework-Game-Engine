#pragma once
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <memory>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include "../Allocator.hpp"
#include "../Char.hpp"
#include "../DynamicArray.hpp"
#include "../Iterators.hpp"
#include "../Operations.hpp"
#include "InitializerList.hpp"

namespace SFTL
{
    namespace Detail
    {
        template<typename T>
        bool IsSpaceChar(T c)
        {
            if constexpr (sizeof(T) == 1)
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            else
                return std::iswspace(static_cast<unsigned short>(c)) != 0;
        }
        template<typename T>
        struct WidenChar
        {
            static T From(unsigned char c) { return static_cast<T>(c); }
        };

        template<typename T>
        constexpr size_type HashSpan(const T *data, size_type count)
        {
            size_type hash            = 14695981039346656037ull;
            constexpr size_type prime = 1099511628211ull;
            for (size_type i = 0; i < count; ++i)
            {
                const auto *bytes = reinterpret_cast<const unsigned char *>(data + i);
                for (size_type b = 0; b < sizeof(T); ++b)
                {
                    hash ^= bytes[b];
                    hash *= prime;
                }
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
        explicit constexpr AdvancedStringView(std::basic_string_view<T> sv) : data_(sv.data()), size_(sv.size()) {}

        constexpr const T *Data() const { return data_; }
        [[nodiscard]] constexpr size_type Size() const { return size_; }
        [[nodiscard]] constexpr bool Empty() const { return size_ == 0; }
        constexpr const T *begin() const { return data_; }
        constexpr const T *end() const { return data_ + size_; }
        constexpr const T &operator[](size_type i) const { return data_[i]; }

        explicit constexpr operator std::basic_string_view<T>() const { return {data_, size_}; }

        constexpr bool operator==(const AdvancedStringView &rhs) const
        {
            return size_ == rhs.size_ && (data_ == rhs.data_ || std::equal(begin(), end(), rhs.begin()));
        }

        constexpr bool operator==(const char *rhs) const
        {
            size_type len = strlen(rhs);
            return size_ == len && std::equal(begin(), end(), rhs);
        }

        constexpr auto operator<=>(const AdvancedStringView &rhs) const
        {
            return std::lexicographical_compare_three_way(begin(), end(), rhs.begin(), rhs.end());
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
            Storage() : inlineBuf{} {}
            ~Storage() {}
        };

        Storage storage_;
        size_type size_     = 0;
        size_type capacity_ = kInlineCapacity;
        Allocator alloc_;

        bool IsHeap() const { return capacity_ > kInlineCapacity; }
        T *Ptr() { return IsHeap() ? storage_.heapBuf : storage_.inlineBuf; }
        const T *Ptr() const { return IsHeap() ? storage_.heapBuf : storage_.inlineBuf; }

        void DestroyHeap()
        {
            if (IsHeap())
                ::SFTL::allocator_traits<Allocator>::deallocate(alloc_, storage_.heapBuf, capacity_ + 1);
        }

        void GrowPreserving(size_type newCapacity)
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

        void AssignRaw(const T *src, size_type count)
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

        void MoveFrom(AdvancedString &other) noexcept
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

        static size_type StrLen(const T *s)
        {
            size_type n = 0;
            while (s[n] != T{})
                ++n;
            return n;
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

        AdvancedString() = default;

        explicit AdvancedString(const Allocator &alloc) : alloc_(alloc) {}

        AdvancedString(size_type count, T ch, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            GrowPreserving(count);
            std::fill_n(Ptr(), count, ch);
            size_        = count;
            Ptr()[size_] = T{};
        }

        explicit AdvancedString(std::span<const T> src, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            AssignRaw(src.data(), src.size());
        }

        AdvancedString(const T *ptr, size_type count, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            AssignRaw(ptr, count);
        }

        AdvancedString(const T *cstr, const Allocator &alloc = Allocator()) : AdvancedString(cstr, StrLen(cstr), alloc)
        {
        }

        AdvancedString(const AdvancedStringView<T> &view, const Allocator &alloc = Allocator()) :
            AdvancedString(view.Data(), view.Size(), alloc)
        {
        }

        template<class InputIt>
        AdvancedString(InputIt first, InputIt last, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        AdvancedString(const AdvancedString &other) : alloc_(other.alloc_) { AssignRaw(other.Ptr(), other.size_); }

        AdvancedString(const AdvancedString &other, size_type pos, size_type count = npos,
                       const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            pos   = std::min(pos, other.size_);
            count = std::min(count, other.size_ - pos);
            AssignRaw(other.Ptr() + pos, count);
        }

        AdvancedString(AdvancedString &&other) noexcept : alloc_(std::move(other.alloc_)) { MoveFrom(other); }

        explicit AdvancedString(initializer_list<T> ilist, const Allocator &alloc = Allocator()) : alloc_(alloc)
        {
            AssignRaw(ilist.begin(), ilist.size());
        }

        explicit AdvancedString(std::string_view *view) { AssignRaw(view->data(), view->size()); }

        AdvancedString &operator=(const AdvancedString &other)
        {
            if (this != &other)
            {
                alloc_ = other.alloc_;
                AssignRaw(other.Ptr(), other.size_);
            }
            return *this;
        }

        AdvancedString &operator=(AdvancedString &&other) noexcept
        {
            if (this != &other)
            {
                DestroyHeap();
                alloc_ = move(other.alloc_);
                MoveFrom(other);
            }
            return *this;
        }

        AdvancedString &operator=(const T *cstr)
        {
            AssignRaw(cstr, StrLen(cstr));
            return *this;
        }
        AdvancedString &operator=(T ch)
        {
            AssignRaw(&ch, 1);
            return *this;
        }

        ~AdvancedString() { DestroyHeap(); }

        reference at(size_type i)
        {
            if (i >= size_)
                throw std::out_of_range("AdvancedString::at");
            return Ptr()[i];
        }
        const_reference at(size_type i) const
        {
            if (i >= size_)
                throw std::out_of_range("AdvancedString::at");
            return Ptr()[i];
        }

        reference operator[](size_type i) { return Ptr()[i]; }
        const_reference operator[](size_type i) const { return Ptr()[i]; }

        reference front() { return Ptr()[0]; }
        const_reference front() const { return Ptr()[0]; }
        reference back() { return Ptr()[size_ - 1]; }
        const_reference back() const { return Ptr()[size_ - 1]; }

        const T *data() const noexcept { return Ptr(); }
        T *data() noexcept { return Ptr(); }
        const T *c_str() const noexcept { return Ptr(); }

        operator std::basic_string_view<T>() const noexcept { return {Ptr(), size_}; }
        std::span<const T> AsSpan() const { return std::span<const T>(Ptr(), size_); }
        AdvancedStringView<T> View() const { return {Ptr(), size_}; }
        operator AdvancedStringView<T>() const { return View(); }

        iterator begin() noexcept { return Ptr(); }
        iterator end() noexcept { return Ptr() + size_; }
        const_iterator begin() const noexcept { return Ptr(); }
        const_iterator end() const noexcept { return Ptr() + size_; }
        const_iterator cbegin() const noexcept { return Ptr(); }
        const_iterator cend() const noexcept { return Ptr() + size_; }

        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] size_type size() const noexcept { return size_; }
        [[nodiscard]] size_type length() const noexcept { return size_; }
        [[nodiscard]] size_type capacity() const noexcept { return capacity_; }
        [[nodiscard]] size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(T) - 1; }

        void reserve(size_type n) { GrowPreserving(n); }

        void shrink_to_fit()
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

        void clear() noexcept
        {
            size_    = 0;
            Ptr()[0] = T{};
        }

        void push_back(T ch)
        {
            GrowPreserving(size_ + 1);
            Ptr()[size_++] = ch;
            Ptr()[size_]   = T{};
        }

        void pop_back()
        {
            if (size_ > 0)
            {
                --size_;
                Ptr()[size_] = T{};
            }
        }

        void resize(size_type count, T ch = T{})
        {
            if (count > size_)
            {
                GrowPreserving(count);
                std::fill(Ptr() + size_, Ptr() + count, ch);
            }
            size_        = count;
            Ptr()[size_] = T{};
        }

        AdvancedString &append(const T *src, size_type count)
        {
            GrowPreserving(size_ + count);
            copy(src, src + count, Ptr() + size_);
            size_ += count;
            Ptr()[size_] = T{};
            return *this;
        }
        AdvancedString &append(const AdvancedStringView<T> &sv) { return append(sv.Data(), sv.Size()); }
        AdvancedString &append(const AdvancedString &other) { return append(other.Ptr(), other.size_); }
        AdvancedString &append(size_type count, T ch)
        {
            GrowPreserving(size_ + count);
            std::fill_n(Ptr() + size_, count, ch);
            size_ += count;
            Ptr()[size_] = T{};
            return *this;
        }

        AdvancedString &operator+=(const AdvancedString &other) { return append(other); }
        AdvancedString &operator+=(const AdvancedStringView<T> &sv) { return append(sv); }
        AdvancedString &operator+=(T ch)
        {
            push_back(ch);
            return *this;
        }
        AdvancedString &operator+=(const T *cstr) { return append(cstr, StrLen(cstr)); }

        iterator insert(const_iterator pos, size_type count, T ch)
        {
            size_type idx = static_cast<size_type>(pos - begin());
            GrowPreserving(size_ + count);
            T *p = Ptr();
            std::move_backward(p + idx, p + size_, p + size_ + count);
            std::fill_n(p + idx, count, ch);
            size_ += count;
            p[size_] = T{};
            return p + idx;
        }

        AdvancedString &insert(size_type index, const T *src, size_type count)
        {
            index = std::min(index, size_);
            GrowPreserving(size_ + count);
            T *p = Ptr();
            std::move_backward(p + index, p + size_, p + size_ + count);
            copy(src, src + count, p + index);
            size_ += count;
            p[size_] = T{};
            return *this;
        }
        AdvancedString &insert(size_type index, const AdvancedStringView<T> &sv)
        {
            return insert(index, sv.Data(), sv.Size());
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            T *p       = Ptr();
            auto start = static_cast<size_type>(first - begin());
            auto count = static_cast<size_type>(last - first);
            std::move(p + start + count, p + size_, p + start);
            size_ -= count;
            p[size_] = T{};
            return p + start;
        }
        iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
        AdvancedString &erase(size_type index = 0, size_type count = npos)
        {
            index = std::min(index, size_);
            count = std::min(count, size_ - index);
            erase(begin() + index, begin() + index + count);
            return *this;
        }

        void swap(AdvancedString &other) noexcept
        {
            AdvancedString tmp(std::move(other));
            other = move(*this);
            *this = move(tmp);
        }

        AdvancedString substr(size_type pos = 0, size_type count = npos) const
        {
            pos   = std::min(pos, size_);
            count = std::min(count, size_ - pos);
            return AdvancedString(Ptr() + pos, count, alloc_);
        }

        // legacy view-returning accessor, prefer substr() for std::string parity
        AdvancedStringView<T> SubstrView(size_type offset, size_type count = npos) const
        {
            offset = std::min(offset, size_);
            count  = std::min(count, size_ - offset);
            return AdvancedStringView<T>(Ptr() + offset, count);
        }

        int compare(const AdvancedString &rhs) const noexcept
        {
            size_type n = std::min(size_, rhs.size_);
            int r       = n ? memcmp(Ptr(), rhs.Ptr(), n * sizeof(T)) : 0;
            if (r != 0)
                return r;
            if (size_ < rhs.size_)
                return -1;
            if (size_ > rhs.size_)
                return 1;
            return 0;
        }
        int compare(const AdvancedStringView<T> &rhs) const noexcept
        {
            return compare(AdvancedString(rhs.Data(), rhs.Size(), alloc_));
        }

        size_type find(T ch, size_type from = 0) const noexcept
        {
            for (size_type i = from; i < size_; ++i)
                if (Ptr()[i] == ch)
                    return i;
            return npos;
        }
        size_type find(const AdvancedStringView<T> &needle, size_type from = 0) const noexcept
        {
            if (needle.Empty())
                return from <= size_ ? from : npos;
            if (needle.Size() > size_)
                return npos;
            for (size_type i = from; i + needle.Size() <= size_; ++i)
                if (std::equal(needle.begin(), needle.end(), Ptr() + i))
                    return i;
            return npos;
        }

        size_type rfind(T ch, size_type from = npos) const noexcept
        {
            if (size_ == 0)
                return npos;
            size_type i = std::min(from, size_ - 1);
            for (;; --i)
            {
                if (Ptr()[i] == ch)
                    return i;
                if (i == 0)
                    break;
            }
            return npos;
        }

        size_type find_first_of(T ch, size_type from = 0) const noexcept { return find(ch, from); }
        size_type find_first_not_of(T ch, size_type from = 0) const noexcept
        {
            for (size_type i = from; i < size_; ++i)
                if (Ptr()[i] != ch)
                    return i;
            return npos;
        }

        bool starts_with(const AdvancedStringView<T> &sv) const noexcept
        {
            return sv.Size() <= size_ && std::equal(sv.begin(), sv.end(), Ptr());
        }
        bool ends_with(const AdvancedStringView<T> &sv) const noexcept
        {
            return sv.Size() <= size_ && std::equal(sv.begin(), sv.end(), Ptr() + size_ - sv.Size());
        }
        bool contains(const AdvancedStringView<T> &sv) const noexcept { return find(sv) != npos; }
        bool contains(T ch) const noexcept { return find(ch) != npos; }

        bool IsSmall() const { return !IsHeap(); }

        AdvancedString Trim() const
        {
            size_type start = 0;
            size_type end_  = size_;
            const T *p      = Ptr();

            while (start < end_ && Detail::IsSpaceChar(p[start]))
                ++start;
            while (end_ > start && Detail::IsSpaceChar(p[end_ - 1]))
                --end_;

            return AdvancedString(p + start, end_ - start, alloc_);
        }

        static AdvancedString FormatV(const char *fmt, va_list args)
        {
            va_list argsSize;
            va_copy(argsSize, args);
            int size = std::vsnprintf(nullptr, 0, fmt, argsSize);
            va_end(argsSize);

            if (size < 0)
                return AdvancedString();

            DynamicArray<char> narrow;
            narrow.resize(static_cast<size_type>(size) + 1);
            std::vsnprintf(narrow.data(), narrow.size(), fmt, args);

            if constexpr (is_same_v<T, char>)
            {
                return AdvancedString(narrow.data(), static_cast<size_type>(size));
            } else
            {
                DynamicArray<T> wide;
                wide.resize(static_cast<size_type>(size));
                for (size_type i = 0; i < static_cast<size_type>(size); ++i)
                    wide[i] = Detail::WidenChar<T>::From(static_cast<unsigned char>(narrow[i]));
                return AdvancedString(wide.data(), static_cast<size_type>(size));
            }
        }

        static AdvancedString Format(const char *fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            AdvancedString result = FormatV(fmt, args);
            va_end(args);
            return result;
        }

        bool operator==(const AdvancedString &rhs) const { return View() == rhs.View(); }
        auto operator<=>(const AdvancedString &rhs) const { return View() <=> rhs.View(); }
        bool operator==(const AdvancedStringView<T> &rhs) const { return View() == rhs; }

        friend AdvancedString operator+(const AdvancedString &lhs, const AdvancedString &rhs)
        {
            AdvancedString result(lhs);
            result.append(rhs);
            return result;
        }
        friend AdvancedString operator+(AdvancedString &&lhs, const AdvancedString &rhs)
        {
            lhs.append(rhs);
            return std::move(lhs);
        }

        friend std::ostream &operator<<(std::ostream &os, const AdvancedString &s)
        {
            if constexpr (is_same_v<T, char>)
                os.write(s.Ptr(), static_cast<std::streamsize>(s.size_));
            return os;
        }

        const T *Data() const { return Ptr(); }
        const T *CStr() const { return Ptr(); }
        size_type Size() const { return size_; }
        size_type Length() const { return size_; }
        size_type Capacity() const { return capacity_; }
        bool Empty() const { return size_ == 0; }
        void Clear() { clear(); }
        void Reserve(size_type n) { reserve(n); }
        AdvancedString &Append(const T *src, size_type count) { return append(src, count); }
        AdvancedString &Append(const AdvancedStringView<T> &sv) { return append(sv); }
        AdvancedString &Append(const AdvancedString &other) { return append(other); }
        AdvancedStringView<T> Substr(size_type offset, size_type count = npos) const
        {
            return SubstrView(offset, count);
        }
        size_type Find(const T &c, size_type from = 0) const { return find(c, from); }
    };

    template<typename T, typename Allocator = allocator<T>>
    AdvancedString<T, Allocator> MakeAdvancedString(std::span<const T> data)
    {
        return AdvancedString<T, Allocator>(data);
    }

    template<typename T, typename Allocator = allocator<T>>
    AdvancedString<T, Allocator> Str()
    {
        return AdvancedString<T, Allocator>();
    }

    template<typename T, typename Allocator = allocator<T>, typename... Args>
    AdvancedString<T, Allocator> ToString(Args &&...args)
    {
        return AdvancedString<T, Allocator>::Format(forward<Args>(args)...);
    }

    // using String = AdvancedString<char>;
    // using WString = AdvancedString<wchar_t>;
} // namespace SFTL

namespace std
{
    template<typename T, typename Allocator>
    struct hash<SFTL::AdvancedString<T, Allocator>>
    {
        size_t operator()(const SFTL::AdvancedString<T, Allocator> &s) const noexcept
        {
            return SFTL::Detail::HashSpan(s.Data(), s.Size());
        }
    };

    template<typename T>
    struct hash<SFTL::AdvancedStringView<T>>
    {
        size_t operator()(const SFTL::AdvancedStringView<T> &s) const noexcept
        {
            return SFTL::Detail::HashSpan(s.Data(), s.Size());
        }
    };
} // namespace std
