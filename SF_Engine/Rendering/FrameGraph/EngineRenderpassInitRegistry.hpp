#pragma once
#include <functional>
#include <vector>

namespace SF::Engine
{
    class EngineRenderpassManager;

    class EngineRenderpassInitRegistry
    {
    public:
        using InitFn = std::function<void(EngineRenderpassManager &)>;

        static EngineRenderpassInitRegistry &Get()
        {
            static EngineRenderpassInitRegistry instance;
            return instance;
        }

        void Register(InitFn fn) { fns_.push_back(std::move(fn)); }

        void RunAll(EngineRenderpassManager &mgr) const
        {
            for (auto &fn: fns_)
                fn(mgr);
        }

    private:
        std::vector<InitFn> fns_;
    };

    template<typename TDerived>
    class PipelinePassAutoInit
    {
        template<typename T, typename = void>
        struct HasPreInit : std::false_type
        {
        };
        template<typename T>
        struct HasPreInit<T, std::void_t<decltype(T::PreInit(std::declval<EngineRenderpassManager &>()))>>
            : std::true_type
        {
        };

        template<typename T, typename = void>
        struct HasInit : std::false_type
        {
        };
        template<typename T>
        struct HasInit<T, std::void_t<decltype(T::Init(std::declval<EngineRenderpassManager &>()))>> : std::true_type
        {
        };

        struct Registrar
        {
            Registrar()
            {
                if constexpr (HasPreInit<TDerived>::value)
                    EngineRenderpassInitRegistry::Get().Register(&TDerived::PreInit);

                if constexpr (HasInit<TDerived>::value)
                    EngineRenderpassInitRegistry::Get().Register(&TDerived::Init);
            }
        };

        inline static Registrar registrar_{};
    };
} // namespace SF::Engine
