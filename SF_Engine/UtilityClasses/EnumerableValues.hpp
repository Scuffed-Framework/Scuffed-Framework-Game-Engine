#pragma once

#include <LowLevel/BitMask.hpp>
#include <type_traits>
#include <utility>

namespace SF::Engine
{
    template <typename E>
        requires std::is_enum_v<std::remove_reference_t<E>>
    class EnumerableValue
    {
    public:
        constexpr EnumerableValue(E value = {}) : value(value) {}

        constexpr operator E() const noexcept
        {
            return value;
        }

        constexpr E operator*() const noexcept
        {
            return value;
        }

        constexpr EnumerableValue &operator=(E value) noexcept
        {
            this->value = value;
            return *this;
        }

    protected:
        E value;
    };

    template <typename E>
    class EnumerableValueRef : public EnumerableValue<E &>
    {
    public:
        constexpr EnumerableValueRef(E &value) : EnumerableValue<E &>(value) {}

        operator const E &() const { return this->value; }
        const E &operator*() const { return this->value; }

        template <typename K>
        constexpr EnumerableValueRef &operator=(const K &value) noexcept
        {
            this->value = value;
            return *this;
        }
    };
}