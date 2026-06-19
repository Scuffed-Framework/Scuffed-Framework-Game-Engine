#pragma once
#include <functional>
#include <vector>

namespace SF::Engine
{
    class PipelinePassManager;

    class PipelinePassInitRegistry
    {
    public:
        using InitFn = std::function<void(PipelinePassManager &)>;

        static PipelinePassInitRegistry &Get()
        {
            static PipelinePassInitRegistry instance;
            return instance;
        }

        void Register(InitFn fn) { fns_.push_back(std::move(fn)); }

        void RunAll(PipelinePassManager &mgr)
        {
            for (auto &fn : fns_)
                fn(mgr);
        }

    private:
        std::vector<InitFn> fns_;
    };

    template <typename TDerived>
    class PipelinePassAutoInit
    {
        template <typename T, typename = void>
        struct HasPreInit : std::false_type
        {
        };
        template <typename T>
        struct HasPreInit<T, std::void_t<decltype(T::PreInit(std::declval<PipelinePassManager &>()))>>
            : std::true_type
        {
        };

        template <typename T, typename = void>
        struct HasInit : std::false_type
        {
        };
        template <typename T>
        struct HasInit<T, std::void_t<decltype(T::Init(std::declval<PipelinePassManager &>()))>>
            : std::true_type
        {
        };

        struct Registrar
        {
            Registrar()
            {
                if constexpr (HasPreInit<TDerived>::value)
                    PipelinePassInitRegistry::Get().Register(&TDerived::PreInit);

                if constexpr (HasInit<TDerived>::value)
                    PipelinePassInitRegistry::Get().Register(&TDerived::Init);
            }
        };

        inline static Registrar registrar_{};
    };
}
#define SF_PIPELINE_PASS_FACTORY(PassType, ...)                   \
    namespace SF::Engine::PassRegistration                        \
    {                                                             \
        static const bool PassType##_registered = []()            \
        {                                                         \
            ::SF::Engine::PipelinePassRegistry::Get().Register(   \
                [](::SF::Engine::PipelinePassManager &mgr)        \
                {                                                 \
                    mgr.Add<PassType>(                            \
                        ::SF::Engine::Pipeline::Stage{0, 0},      \
                        std::make_unique<PassType>(__VA_ARGS__)); \
                });                                               \
            return true;                                          \
        }();                                                      \
    }