/******************************************************************************/
/* KVP.hpp                                                                    */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
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
#include <utility>
#include <tuple>

namespace SF::Engine
{
    template <typename Key, typename Value>
    struct KeyValuePair
    {
        Key key;
        Value value;

        // Default constructor
        KeyValuePair() = default;

        // Constructor with key and value
        KeyValuePair(const Key &k, const Value &v) : key(k), value(v) {}

        // Move constructor
        KeyValuePair(Key &&k, Value &&v)
            : key(std::move(k)), value(std::move(v)) {}

        // Copy constructor
        KeyValuePair(const KeyValuePair &other) = default;

        // Move constructor
        KeyValuePair(KeyValuePair &&other) noexcept = default;

        // Assignment operators
        KeyValuePair &operator=(const KeyValuePair &other) = default;
        KeyValuePair &operator=(KeyValuePair &&other) noexcept = default;

        // Comparison operators (compare by key only)
        bool operator==(const KeyValuePair &other) const { return key == other.key; }
        bool operator!=(const KeyValuePair &other) const { return !(*this == other); }
        bool operator<(const KeyValuePair &other) const { return key < other.key; }
        bool operator>(const KeyValuePair &other) const { return key > other.key; }
        bool operator<=(const KeyValuePair &other) const { return key <= other.key; }
        bool operator>=(const KeyValuePair &other) const { return key >= other.key; }

        // Structured binding support
        template <std::size_t I>
        auto &get()
        {
            if constexpr (I == 0)
                return key;
            else if constexpr (I == 1)
                return value;
        }

        template <std::size_t I>
        const auto &get() const
        {
            if constexpr (I == 0)
                return key;
            else if constexpr (I == 1)
                return value;
        }

        // Conversion operator to std::pair (if needed)
        operator std::pair<Key, Value>() const
        {
            return std::make_pair(key, value);
        }

        // Swap function
        void swap(KeyValuePair &other) noexcept
        {
            using std::swap;
            swap(key, other.key);
            swap(value, other.value);
        }

        // Factory function for creating with perfect forwarding
        template <typename K, typename V>
        static KeyValuePair make(K &&k, V &&v)
        {
            return KeyValuePair(std::forward<K>(k), std::forward<V>(v));
        }

        // Get first and second (compatibility with std::pair)
        const Key &first() const { return key; }
        Key &first() { return key; }

        const Value &second() const { return value; }
        Value &second() { return value; }
    };

    // Deduction guide
    template <typename K, typename V>
    KeyValuePair(K, V) -> KeyValuePair<K, V>;

    // Swap specialization
    template <typename Key, typename Value>
    void swap(KeyValuePair<Key, Value> &lhs, KeyValuePair<Key, Value> &rhs) noexcept
    {
        lhs.swap(rhs);
    }
}

// Specialization for std::tuple_size and std::tuple_element for structured bindings
namespace std
{
    template <typename Key, typename Value>
    struct tuple_size<SF::Engine::KeyValuePair<Key, Value>>
        : integral_constant<size_t, 2>
    {
    };

    template <size_t I, typename Key, typename Value>
    struct tuple_element<I, SF::Engine::KeyValuePair<Key, Value>>
    {
        using type = conditional_t<I == 0, Key, Value>;
    };
}