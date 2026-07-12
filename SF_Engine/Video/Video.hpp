#pragma once
#include <Engine/Module.hpp>
#include <Audio/AudioClip.hpp> // decode an audio track for the video

namespace SF::Engine
{
    class Video : public ModuleRegistrar<Video>
    {
        inline static const bool registered = Register(Stage::Pre);

    public:
        Video();
        ~Video();
    };
}