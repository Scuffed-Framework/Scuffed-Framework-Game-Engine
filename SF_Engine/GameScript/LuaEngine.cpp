#include "LuaEngine.hpp"
#include <iostream>

namespace SF::Engine::Scripting::Lua
{
    void LuaEngine::Init()
    {
        lua_.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::table,
            sol::lib::string,
            sol::lib::coroutine);
    }

    void LuaEngine::Shutdown()
    {
        loadedModules_.clear();
        lua_.collect_garbage();
    }
}