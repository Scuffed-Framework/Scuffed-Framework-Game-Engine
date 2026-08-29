#include "LuaEngine.hpp"
#include <iostream>

namespace SF::Engine::Scripting::Lua
{
    bool LuaEngine::Initialize()
    {
        lua_.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::table,
            sol::lib::string,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::io,
            sol::lib::utf8
            );
        return true;
    }

    void LuaEngine::Shutdown()
    {
        loadedModules_.clear();
        lua_.collect_garbage();
    }
}