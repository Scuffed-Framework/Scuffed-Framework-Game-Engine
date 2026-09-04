#pragma once


#include <Engine/Module.hpp>

#ifdef Always
    #undef Always
#endif
namespace SF::Engine::Scripting::Lua
{
    class LuaEngine : public ModuleRegistrar<LuaEngine>
    {
        friend class ModuleRegistrar<LuaEngine>;
        REGISTER_MODULE(LuaEngine, Module::Stage::Always); // Or at least I think this is right

    public:
        bool Initialize() override;
        void Shutdown() override;

        void Update() override {

        };

    private:
    };
} // namespace SF::Engine::Scripting::Lua
