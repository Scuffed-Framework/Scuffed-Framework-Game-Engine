#pragma once

#include <sol/sol.hpp>

#include <Engine/Module.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <functional>
#include <vector>
#include <optional>

#include <Commands/CommandsWindow.hpp>

namespace SF::Engine::Scripting::Lua
{
    class LuaEngine : public ModuleRegistrar<LuaEngine>
    {
        friend class ModuleRegistrar<LuaEngine>;
        REGISTER_MODULE(LuaEngine, Module::Stage::Always); // Or at least I think this is right

    public:
        void Init();
        void Shutdown();

        void Update() override 
        {

        };

    private:
        sol::state lua_;

        std::unordered_map<std::string, sol::table> loadedModules_;
    };
}