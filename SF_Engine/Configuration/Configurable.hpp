#include <Delegates/Delegate.hpp>

template <typename TDerived, typename TContainer>
class Configurable
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