#pragma once
#include <Configuration/Configurable.hpp>

namespace SF::Engine
{
    struct GameModeBase //: public Configurable
    {
        virtual ~GameModeBase() = default;

        // Override to implement game mode behavior
        virtual void OnStart() {}
        virtual void OnStop() {}
    };
}