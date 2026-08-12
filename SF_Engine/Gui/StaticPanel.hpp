#pragma once
namespace SF::Engine
{
    template<typename T>
    class StaticSingleInstancePanel
    {
    public:
        static T &Get()
        {
            return *s_instance;
        }

    protected:
        StaticSingleInstancePanel()
        {
            s_instance = static_cast<T *>(this);
        }

        ~StaticSingleInstancePanel()
        {
            s_instance = nullptr;
        }

    private:
        inline static T *s_instance = nullptr;
    };
}