#pragma once
#include <EntityComponentSystem/Entity.hpp>
#include <Rendering/Camera/Camera.hpp>

namespace SF::Engine
{
    // entity
    class PlayerCamera : public Camera
    {
    private:
        bool UseInfiniteFarPlane;

    public:
    };
} // namespace SF::Engine
