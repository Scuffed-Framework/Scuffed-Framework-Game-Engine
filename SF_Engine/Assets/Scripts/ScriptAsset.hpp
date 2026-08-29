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

        void Save() override
        {

        }

        bool Load(std::span<const uint8_t>) override
        {
            return true;
        }
    };
}