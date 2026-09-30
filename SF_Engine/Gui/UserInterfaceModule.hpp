#pragma once
#include <Engine/Module.hpp>

namespace SF::Engine
{
    using namespace std;
    class UserInterfaceModule : public ModuleRegistrar<UserInterfaceModule>
    {
        REGISTER_MODULE(UserInterfaceModule, Module::Stage::Pre);

    public:
        ~UserInterfaceModule() override = default;
        void Update() override {}
        bool Initialize() override { return true; }
    };
} // namespace SF::Engine
