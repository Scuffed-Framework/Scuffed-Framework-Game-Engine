#pragma once
#include <memory>
#include <TemplateLibrary/Any.hpp>

namespace SF::Engine
{
    class Controller
    {
    public:
        virtual ~Controller() = default;
        virtual void Update(float dt) {}
        virtual void Initialize() {}
        virtual void Shutdown() {}
    };

    template <typename T>
    class StaticController : public Controller
    {
    public:
        static T &Get()
        {
            return *s_instance;
        }

    protected:
        StaticController()
        {
            s_instance = static_cast<T *>(this);
        }

        ~StaticController()
        {
            s_instance = nullptr;
        }

    private:
        inline static T *s_instance = nullptr;
    };
}