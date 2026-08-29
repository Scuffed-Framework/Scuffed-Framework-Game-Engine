#pragma once
#include <Configuration/Configurable.hpp>
#include <Scene/Scene.hpp>
#include <Engine/Log/Log.hpp>

namespace SF::Engine
{
    struct LogConfig
    {
        bool shutUp;
    };
    struct EngineConfig : Serializable
    {
    public:
        Scene *StartupScene;
        void Serialize(XMLNode &node) const override
        {
            auto startup = node.AddChild("StartupScene");
            startup.SetContent(
                StartupScene ? StartupScene->GetName() : "");
        }

        void Deserialize(const XMLNode &node) override
        {
        }
    };
} // namespace SF::Engine
