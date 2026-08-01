#pragma once
#include <Graphics/Lighting/Light.hpp>

namespace SF::Engine
{
    struct LightComponent : public Serializable
    {
        Light light;

        void Serialize(XMLNode &node) const override { light.Serialize(node); }
        void Deserialize(const XMLNode &node) override { light.Deserialize(node); }
    };
}