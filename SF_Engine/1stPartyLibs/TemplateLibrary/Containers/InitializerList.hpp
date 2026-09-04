#pragma once
#include "../Types.hpp"

namespace SFTL
{
    template <class C>
    class initializer_list
    {
    public:
        typedef C value_type;
        typedef const C &reference;
        typedef const C &const_reference;
        typedef const C *iterator;
        typedef const C *const_iterator;

    private:
        iterator arr;
        size_type len;

        constexpr initializer_list(const_iterator arr2, size_type len2)
            : arr(arr2), len(len2) {}

    public:
        constexpr initializer_list() noexcept
            : arr(0), len(0) {}

        constexpr size_type size() const noexcept { return len; }

        constexpr const_iterator begin() const noexcept { return arr; }

        constexpr const_iterator end() const noexcept { return begin() + size(); }
    };

    template <class T>
    constexpr const T * begin(initializer_list<T> list) noexcept
    {
        return list.begin();
    }

    template <class T> constexpr const T *
    end(initializer_list<T> list) noexcept
    {
        return list.end();
    }
}