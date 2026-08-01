#pragma once

namespace SF::Engine
{
    template <typename Derived, typename Product>
    class BuilderPattern
    {
    public:
        constexpr Derived &Self() noexcept
        {
            return static_cast<Derived &>(*this);
        }

        constexpr const Derived &Self() const noexcept
        {
            return static_cast<const Derived &>(*this);
        }

        constexpr Product Build()
        {
            return std::move(m_Product);
        }

    protected:
        Product m_Product{};
    };
}