#pragma once
#include <Configuration/Configurable.hpp>
#include <Engine/Log/Log.hpp>
#include <Scene/Scene.hpp>

namespace SF::Engine
{
    struct LogConfig
    {
        bool shutUp;
    };
    struct EngineConfig : Serializable
    {
    public:
        unsigned int LongDoubleSize = sizeof(long double);
        Scene *StartupScene;


        void Serialize(XMLNode &node) const override
        {
            auto startup = node.AddChild("StartupScene");
            startup.SetContent(StartupScene ? StartupScene->GetName() : "");
        }

        void Deserialize(const XMLNode &node) override
        {
            // once we've loaded the game binaries and searched the rscs for the EngineConfig then we'll run
            // Engine::Get()->Configure(cfg); // not ref cuz the function should unload cfg
        }
    };
} // namespace SF::Engine
