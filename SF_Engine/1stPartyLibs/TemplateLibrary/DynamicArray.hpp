/******************************************************************************/
/* DynamicArray.hpp                                                           */
/******************************************************************************/
/*                            This file is part of                            */
/*             Scuffed Framework Standard Template Library                    */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
#pragma once
#include <cassert>
#include "Allocator.hpp"
#include "TypeTraits.hpp"

namespace SFTL
{
    template<typename T, class Allocator = allocator<T>>
    class DynamicArray
    {
    public:
        using value_type     = T;
        using allocator_type = Allocator;

        DynamicArray() = default;

        explicit DynamicArray(const Allocator &alloc) : allocator_(alloc) {}

        ~DynamicArray()
        {
            clear();
            if (data_)
                allocator_.deallocate(data_, capacity_);
        }

        // move is cheap and sane
        DynamicArray(DynamicArray &&other) noexcept { steal(other); }

        DynamicArray &operator=(DynamicArray &&other) noexcept
        {
            if (this != &other)
            {
                this->~DynamicArray();
                steal(other);
            }
            return *this;
        }

        T &operator[](size_type index)
        {
            assert(index < size_);
            return data_[index];
        }

        const T &operator[](size_type index) const
        {
            assert(index < size_);
            return data_[index];
        }

        [[nodiscard]] size_type size() const noexcept { return size_; }
        [[nodiscard]] size_type capacity() const noexcept { return capacity_; }


        void clear()
        {
            for (size_type i = 0; i < size_; ++i)
                allocator_traits<Allocator>::destroy(allocator_, data_ + i);

            size_ = 0;
        }

        constexpr void swap(DynamicArray &right) noexcept(noexcept(::SFTL::swap(data_, right.data_)) &&
                                                          noexcept(::SFTL::swap(size_, right.size_)) &&
                                                          noexcept(::SFTL::swap(capacity_, right.capacity_)) &&
                                                          noexcept(::SFTL::swap(allocator_, right.allocator_)))
        {
            if (this != ::SFTL::addressof(right))
            {
                ::SFTL::swap(data_, right.data_);
                ::SFTL::swap(size_, right.size_);
                ::SFTL::swap(capacity_, right.capacity_);
                ::SFTL::swap(allocator_, right.allocator_);
            }
        }

        friend bool operator==(const DynamicArray &a, const DynamicArray &b)
        {
            if (a.size_ != b.size_)
                return false;

            for (size_type i = 0; i < a.size_; ++i)
            {
                if (!(a.data_[i] == b.data_[i]))
                    return false;
            }
            return true;
        }

    private:
        T *data_            = nullptr;
        size_type size_     = 0;
        size_type capacity_ = 0;
        Allocator allocator_;

        void grow()
        {
            size_type newCapacity = capacity_ == 0 ? 4 : capacity_ * 2;
            T *newData            = allocator_.allocate(newCapacity);

            for (size_type i = 0; i < size_; ++i)
            {
                allocator_traits<Allocator>::construct(allocator_, newData + i, ::SFTL::move_if_noexcept(data_[i]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i);
            }

            if (data_)
                allocator_.deallocate(data_, capacity_);

            data_     = newData;
            capacity_ = newCapacity;
        }

        void steal(DynamicArray &other) noexcept
        {
            data_      = other.data_;
            size_      = other.size_;
            capacity_  = other.capacity_;
            allocator_ = ::SFTL::move(other.allocator_);

            other.data_     = nullptr;
            other.size_     = 0;
            other.capacity_ = 0;
        }

    public:
        void reserve(size_type newCapacity)
        {
            if (newCapacity <= capacity_)
                return;

            T *newData = allocator_.allocate(newCapacity);

            for (size_type i = 0; i < size_; ++i)
            {
                allocator_traits<Allocator>::construct(allocator_, newData + i, ::SFTL::move(data_[i]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i);
            }

            if (data_)
                allocator_.deallocate(data_, capacity_);

            data_     = newData;
            capacity_ = newCapacity;
        }
        void pop_back()
        {
            assert(size_ > 0);

            --size_;
            allocator_traits<Allocator>::destroy(allocator_, data_ + size_);
        }
        T *data() noexcept { return data_; }
        const T *data() const noexcept { return data_; }
        T *begin() noexcept { return data_; }
        T *end() noexcept { return data_ + size_; }

        const T *begin() const noexcept { return data_; }
        const T *end() const noexcept { return data_ + size_; }
        DynamicArray(const DynamicArray &other) :
            allocator_(allocator_traits<Allocator>::select_on_container_copy_construction(other.allocator_))
        {
            reserve(other.size_);

            for (size_type i = 0; i < other.size_; ++i)
                push_back(other.data_[i]);
        }
        DynamicArray &operator=(const DynamicArray &other)
        {
            if (this == &other)
                return *this;

            DynamicArray temp(other);
            swap(*this, temp);
            return *this;
        }

        void push_back(const T &value)
        {
            if (size_ == capacity_)
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);

            allocator_traits<Allocator>::construct(allocator_, data_ + size_, value); // copy, not move
            ++size_;
        }

        void push_back(T &&value)
        {
            if (size_ == capacity_)
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);

            allocator_traits<Allocator>::construct(allocator_, data_ + size_, ::SFTL::move(value));
            ++size_;
        }

        template<typename... Args>
        void emplace_back(Args &&...args)
        {
            if (size_ == capacity_)
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);

            allocator_traits<Allocator>::construct(allocator_, data_ + size_, ::SFTL::forward<Args>(args)...);
            ++size_;
        }

        void resize(size_type newSize)
        {
            if (newSize > capacity_)
                reserve(newSize);

            if (newSize > size_)
            {
                for (size_type i = size_; i < newSize; ++i)
                    allocator_traits<Allocator>::construct(allocator_, data_ + i);
            } else if (newSize < size_)
            {
                for (size_type i = newSize; i < size_; ++i)
                    allocator_traits<Allocator>::destroy(allocator_, data_ + i);
            }

            size_ = newSize;
        }

        void resize(size_type newSize, const T &value)
        {
            if (newSize > capacity_)
                reserve(newSize);

            if (newSize > size_)
            {
                for (size_type i = size_; i < newSize; ++i)
                    allocator_traits<Allocator>::construct(allocator_, data_ + i, value);
            } else if (newSize < size_)
            {
                for (size_type i = newSize; i < size_; ++i)
                    allocator_traits<Allocator>::destroy(allocator_, data_ + i);
            }

            size_ = newSize;
        }

        bool empty() const noexcept { return size_ == 0; }

        T &front()
        {
            assert(size_ > 0);
            return data_[0];
        }

        const T &front() const
        {
            assert(size_ > 0);
            return data_[0];
        }

        T &back()
        {
            assert(size_ > 0);
            return data_[size_ - 1];
        }

        const T &back() const
        {
            assert(size_ > 0);
            return data_[size_ - 1];
        }

        void shrink_to_fit()
        {
            if (size_ == capacity_)
                return;

            if (size_ == 0)
            {
                if (data_)
                    allocator_.deallocate(data_, capacity_);
                data_     = nullptr;
                capacity_ = 0;
                return;
            }

            T *newData = allocator_.allocate(size_);

            for (size_type i = 0; i < size_; ++i)
            {
                allocator_traits<Allocator>::construct(allocator_, newData + i, ::SFTL::move_if_noexcept(data_[i]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i);
            }

            allocator_.deallocate(data_, capacity_);
            data_     = newData;
            capacity_ = size_;
        }

        T *insert(T *pos, const T &value)
        {
            assert(pos >= begin() && pos <= end());
            size_type index = pos - data_;

            if (size_ == capacity_)
            {
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);
            }

            for (size_type i = size_; i > index; --i)
            {
                allocator_traits<Allocator>::construct(allocator_, data_ + i, ::SFTL::move_if_noexcept(data_[i - 1]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i - 1);
            }

            allocator_traits<Allocator>::construct(allocator_, data_ + index, value);
            ++size_;

            return data_ + index;
        }

        T *insert(T *pos, T &&value)
        {
            assert(pos >= begin() && pos <= end());
            size_type index = pos - data_;

            if (size_ == capacity_)
            {
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);
            }

            for (size_type i = size_; i > index; --i)
            {
                allocator_traits<Allocator>::construct(allocator_, data_ + i, ::SFTL::move_if_noexcept(data_[i - 1]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i - 1);
            }

            allocator_traits<Allocator>::construct(allocator_, data_ + index, ::SFTL::move(value));
            ++size_;

            return data_ + index;
        }

        T *erase(T *pos)
        {
            assert(pos >= begin() && pos < end());
            size_type index = pos - data_;

            allocator_traits<Allocator>::destroy(allocator_, data_ + index);

            for (size_type i = index; i < size_ - 1; ++i)
            {
                allocator_traits<Allocator>::construct(allocator_, data_ + i, ::SFTL::move_if_noexcept(data_[i + 1]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i + 1);
            }

            --size_;
            return data_ + index;
        }

        T *erase(T *first, T *last)
        {
            assert(first >= begin() && first <= end());
            assert(last >= first && last <= end());

            if (first == last)
                return first;

            size_type startIndex = first - data_;
            size_type count      = last - first;

            for (size_type i = startIndex; i < startIndex + count; ++i)
                allocator_traits<Allocator>::destroy(allocator_, data_ + i);

            for (size_type i = startIndex; i < size_ - count; ++i)
            {
                allocator_traits<Allocator>::construct(allocator_, data_ + i,
                                                       ::SFTL::move_if_noexcept(data_[i + count]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + i + count);
            }

            size_ -= count;
            return data_ + startIndex;
        }

        friend void swap(DynamicArray &a, DynamicArray &b) noexcept
        {
            ::SFTL::swap(a.data_, b.data_);
            ::SFTL::swap(a.size_, b.size_);
            ::SFTL::swap(a.capacity_, b.capacity_);
            ::SFTL::swap(a.allocator_, b.allocator_);
        }

        friend bool operator!=(const DynamicArray &a, const DynamicArray &b) { return !(a == b); }
        // Remove all elements matching a value (maintains order)
        size_type remove(const T &value)
        {
            T *writePos       = data_;
            size_type removed = 0;

            for (size_type i = 0; i < size_; ++i)
            {
                if (data_[i] == value)
                {
                    allocator_traits<Allocator>::destroy(allocator_, data_ + i);
                    ++removed;
                } else
                {
                    if (writePos != data_ + i)
                    {
                        allocator_traits<Allocator>::construct(allocator_, writePos,
                                                               ::SFTL::move_if_noexcept(data_[i]));
                        allocator_traits<Allocator>::destroy(allocator_, data_ + i);
                    }
                    ++writePos;
                }
            }

            size_ -= removed;
            return removed;
        }

        // Fast unordered removal by swapping with last element
        void swap_remove(size_type index)
        {
            assert(index < size_);

            if (index != size_ - 1)
            {
                allocator_traits<Allocator>::destroy(allocator_, data_ + index);
                allocator_traits<Allocator>::construct(allocator_, data_ + index,
                                                       ::SFTL::move_if_noexcept(data_[size_ - 1]));
                allocator_traits<Allocator>::destroy(allocator_, data_ + size_ - 1);
            } else
            {
                allocator_traits<Allocator>::destroy(allocator_, data_ + index);
            }

            --size_;
        }

        // Fast unordered removal by iterator
        T *swap_remove(T *pos)
        {
            assert(pos >= begin() && pos < end());
            swap_remove(pos - data_);
            return pos; // Note: element at pos is now different
        }
    };

    //    namespace Polymorphic
    //  {
    //    template<class T>
    //  using DynamicArray = ::SFTL::DynamicArray<T, polymorphic_allocator<T>>;
    //}
} // namespace SFTL
