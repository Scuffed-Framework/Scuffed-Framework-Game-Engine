#pragma once
#include <Assets/AssetPipeline.hpp>

namespace SF::Engine
{
    class ShaderAsset : public AssetBase
    {
        SF_RTTI(ShaderAsset, AssetBase)
    public:
        std::filesystem::path shaderPath;

        void Save() override
        {
        }

        bool Load(std::span<const uint8_t>) override
        {
            return true;
        }
    };
}