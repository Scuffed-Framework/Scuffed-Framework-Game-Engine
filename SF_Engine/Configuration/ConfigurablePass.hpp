namespace SF::Engine
{
    class Configurable
    {
    private:
        bool enabled_ = true;

    public:
        virtual ~Configurable() = 0;

        // Toggle
        void Enable() noexcept
        {
            if (!enabled_)
            {
                enabled_ = true;
                OnEnable();
            }
        }
        void Disable() noexcept
        {
            if (enabled_)
            {
                enabled_ = false;
                OnDisable();
            }
        }

        // Query
        bool IsEnabled() const noexcept { return enabled_; }

    protected:
        // Override only if needed
        virtual void OnEnable() {}
        virtual void OnDisable() {}
    };
}