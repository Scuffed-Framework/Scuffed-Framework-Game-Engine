#pragma once
#include <XML/XMLReader.hpp>
#include <Scene/Scene.hpp>

namespace SF::Engine
{
    struct GlobalEngineSettings : public Serializable
    {
    public:
        // Editor
        bool isEditor = false;

        // Engine startup
    };
}