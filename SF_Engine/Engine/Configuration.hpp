#pragma once
#include <Configuration/Configurable.hpp>
#include <Engine/Log/Log.hpp>
#include <Scene/Scene.hpp>

namespace SF::Engine
{
    struct EngineConfig : Serializable
    {
    public:
        struct LogConfig
        {
            bool Disable;
        };

        // Scalable Multi-threaded Adaptive Rendering Technology
        struct SMARTRenderSystemConfig
        {
            bool VulkanValidationLayersAllowed;
        };

        unsigned int LongDoubleSize = sizeof(long double);
        Scene *StartupScene;


        void Serialize(XMLNode &node) const override
        {
            auto startup = node.AddChild("StartupScene");
            startup.SetContent(StartupScene ? StartupScene->GetName() : "");
        }

        void Deserialize(const XMLNode &) override
        {
            // once we've loaded the game binaries and searched the rscs for the EngineConfig then we'll run
            // Engine::Get()->Configure(cfg); // not ref cuz the function should unload cfg
        }
    };
} // namespace SF::Engine
