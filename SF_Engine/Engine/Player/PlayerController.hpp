#pragma once
#include <Controllers/CameraController.hpp>
#include "PlayerCamera.hpp"

#include <memory>

namespace SF::Engine
{
    // virtual controller cuz this needs its own update, init/shutdown
    class PlayerController : public virtual Controller // NOT STATIC CONTROLLER, MANY PCs MAY EXIST AT ONCE, EG:
    // SpectatorPC, InGamePC
    // should provide Update(float deltatime), Initialize, Shutdown
    {
    public:
        std::unique_ptr<PlayerCamera> m_Camera; // decided to use this format blah blah i forgor what it is called because Camera is a class.
    };
}