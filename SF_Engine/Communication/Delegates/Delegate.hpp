#include <functional>
#include <vector>

namespace SF::Engine
{
    template <typename... Args>
    class Delegate
    {
    public:
        using Function = std::function<void(Args...)>;

        void Add(Function fn)
        {
            listeners_.push_back(std::move(fn));
        }

        void Broadcast(Args... args)
        {
            for (auto &fn : listeners_)
                fn(args...);
        }

    private:
        std::vector<Function> listeners_;
    };
}