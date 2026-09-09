#pragma once
#include <string>

#include <Engine/VersionSemantic.hpp>

namespace SF::Engine
{
    using namespace std;
    struct GameInfo
    {
    public:
        const string name;
        Version version;
    };
} // namespace SF::Engine
