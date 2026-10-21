#pragma once
#include <Configuration/Configurable.hpp>
#include <Scene/Scene.hpp>
#include "InitGame/AppInfo.hpp"

namespace SF::Engine
{
    class Version;
    struct EngineConfig : Serializable
    {
    public:
        struct LogConfig
        {
            bool Disable;
        };

        struct BuildConfiguration
        {
            bool RSC_ENCRYPTION          = true;
            bool RSC_SEGMENT_KEYS        = true;
            bool RSC_MANIFEST_ENCRYPTION = true;
            bool RSC_SIGNATURES          = true;
            bool RSC_OBFUSCATION         = true;
            bool RSC_MEMORY_PROTECTION   = false;
            bool RSC_ANTICHEAT           = false;
            bool RSC_STREAMING           = true;

            AppInfo Engine;
            AppInfo Game;
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
