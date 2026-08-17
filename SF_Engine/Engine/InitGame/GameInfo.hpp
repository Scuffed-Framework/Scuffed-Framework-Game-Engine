#pragma once
#include <string>
#include <TemplateLibrary/Types.hpp>

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