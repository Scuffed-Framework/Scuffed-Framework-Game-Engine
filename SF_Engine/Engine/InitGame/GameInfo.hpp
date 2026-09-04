#pragma once
#include <1stPartyLibs/TemplateLibrary/Types.hpp>
#include <string>

#include <Engine/VersionSemantic.hpp>

namespace SF::Engine
{
    struct GameInfo
    {
    public:
        const std::string name;
        Version version;
    };
}