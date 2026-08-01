#pragma once
#include <TemplateLibrary/Containers/AdvancedString.hpp>
#include <TemplateLibrary/Types.hpp>

#include <Engine/VersionSemantic.hpp>

namespace SF::Engine
{
    struct GameInfo
    {
    public:
        const ::SFTL::String name;
        Version version;
    };
}