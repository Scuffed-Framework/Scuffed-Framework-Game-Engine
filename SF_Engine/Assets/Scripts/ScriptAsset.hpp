#pragma once
#include <Assets/AssetPipeline.hpp>

namespace SF::Engine
{
    enum ScriptType
    {
        CXX,
        Lua
    };

    class ScriptAsset : public AssetBase
    {
        SF_RTTI(ScriptAsset, AssetBase)
    public:
        std::filesystem::path scriptPath;
        ScriptType type;

        void Save() override {}

        void Serialize(XMLNode &node) const override
        {
            XMLNode script = node.AddChild("Script");
            script.SetAttribute("Path", &scriptPath);
            script.SetAttribute("Type", static_cast<int>(type));
        }

        void Deserialize(const XMLNode &node) override
        {
            XMLNode script = node.GetChild("Script");
            scriptPath     = script.GetAttribute("Path");
            int rawType{};
            script.GetAttribute("Type", rawType);
            type = static_cast<ScriptType>(rawType);
        }

        bool Load(std::span<const uint8_t>) override { return true; }
    };
} // namespace SF::Engine
