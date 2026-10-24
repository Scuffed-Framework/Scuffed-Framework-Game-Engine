#pragma once

namespace SF::Engine
{
    class Controller
    {
    public:
        virtual ~Controller() = default;
        virtual void Update(float /*DeltaTime*/) {}
        virtual void Initialize() {}
        virtual void Shutdown() {}
    };

    template<typename T>
    class StaticController : public Controller
    {
    public:
        static T &Get() { return *instance; }

    protected:
        StaticController() { instance = static_cast<T *>(this); }

        ~StaticController()
        {
            if (instance == this)
                instance = nullptr;
        }

    private:
        inline static T *instance = nullptr;
    };
} // namespace SF::Engine
