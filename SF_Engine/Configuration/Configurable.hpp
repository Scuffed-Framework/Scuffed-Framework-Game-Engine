#pragma once
#include <Delegates/Delegate.hpp>
#include <LowLevel/XML/XMLModule.hpp>

template <typename TDerived, typename TContainer>
class Configurable : public Serializable
{
public:
    void Enable() noexcept
    {
        if (!enabled_)
        {
            enabled_ = true;

            if constexpr (requires(TDerived &d) { d.OnEnable(); })
                Derived().OnEnable();
        }
    }

    void Disable() noexcept
    {
        if (enabled_)
        {
            enabled_ = false;

            if constexpr (requires(TDerived &d) { d.OnDisable(); })
                Derived().OnDisable();
        }
    }

    bool IsEnabled() const noexcept
    {
        return enabled_;
    }

    Delegate<const TDerived &, const TContainer &> OnConfigChanged;

    void Serialize(XMLNode &node) const override
    {
        node.SetAttribute("enabled", enabled_);

        if constexpr (requires(const TDerived &d, XMLNode &n) { d.OnSerialize(n); })
            Derived().OnSerialize(node);
    }

    void Deserialize(const XMLNode &node) override
    {
        if (const auto val = node.GetAttribute<bool>("enabled"))
            enabled_ = *val;

        if constexpr (requires(TDerived &d, const XMLNode &n) { d.OnDeserialize(n); })
            Derived().OnDeserialize(node);

        if (enabled_)
        {
            if constexpr (requires(TDerived &d) { d.OnEnable(); })
                Derived().OnEnable();
        }
        else
        {
            if constexpr (requires(TDerived &d) { d.OnDisable(); })
                Derived().OnDisable();
        }
    }

protected:
    void NotifyConfigChanged()
    {
        OnConfigChanged.Broadcast(
            static_cast<const TDerived &>(*this),
            static_cast<const TContainer &>(*this));
    }

    ~Configurable() = default;

private:
    TDerived &Derived() noexcept
    {
        return static_cast<TDerived &>(*this);
    }

    const TDerived &Derived() const noexcept
    {
        return static_cast<const TDerived &>(*this);
    }

    bool enabled_ = true;
};