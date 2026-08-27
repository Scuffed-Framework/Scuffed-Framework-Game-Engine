namespace SF::Engine
{
    template <typename... Args>
    class MulticastDelegate
    {
    public:
        using Callback = std::function<void(Args...)>;

        template <typename Callable>
        void Add(Callable &&callable)
        {
            callbacks_.emplace_back(std::forward<Callable>(callable));
        }

        void Broadcast(Args... args)
        {
            for (auto &callback : callbacks_)
            {
                callback(args...);
            }
        }

        bool IsBound() const noexcept
        {
            return !callbacks_.empty();
        }

    private:
        std::vector<Callback> callbacks_;
    };
}