#pragma once

#include <XML/XMLReader.hpp>
#include <ID/GUID.hpp>

namespace SF::Engine
{
    enum AssetType
    {
        Mesh,
        Texture,
        Shader,
        Script,  // Lua
        CppCode, // we should be able to run C++ code like in unreal
        VFX,
        Audio,
        SkeletalAnimation,
        Scene,
        TemplateAsset,
        ConfigFile,
        Font,
        Material,
        Library, // Dll, or rsc file
        AnimationStateMachine,
        Text
    };

    struct Asset : Serializable // asset.png serializes to: asset.xml, this also is an interface
    {
        std::string name;
        AssetType type;
        GUID guid;
    };
}