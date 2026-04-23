#pragma once

#include "LightingTypes.hpp"
#include <Graphics/Material/Color/Color.hpp>
#include <Math/Vectors/Vector.hpp>
#include <glm/glm.hpp>
#include <string>
#include <XML/XMLReader.hpp>
#include <Scene/SceneSerialization.hpp>

namespace SF::Engine
{
    /**
     * @brief CPU-side light descriptor.  Add these to a LightSystem (or build
     *        the GpuLight array yourself) and upload via LightManager.
     */
    struct Light : public Serializable
    {
        Lighting::LightType type = Lighting::LightType::Point;

        glm::vec3 position = {0, 0, 0};
        glm::vec3 direction = {0, -1, 0}; // normalised, pointing away from source
        glm::vec3 color = {1, 1, 1};      // linear RGB
        float intensity = 1.0f;           // candela / lux
        float radius = 10.0f;             // effective range (point/spot)
        float innerConeAngleDeg = 30.0f;  // spot inner
        float outerConeAngleDeg = 45.0f;  // spot outer
        bool castShadow = false;

        std::string name;

        /// Convert to GPU-ready struct
        Lighting::GpuLight ToGpu() const
        {
            Lighting::GpuLight g{};
            g.position = position;
            g.radius = radius;
            g.color = color;
            g.intensity = intensity;
            g.direction = glm::normalize(direction);
            g.innerConeAngle = glm::cos(glm::radians(innerConeAngleDeg));
            g.outerConeAngle = glm::cos(glm::radians(outerConeAngleDeg));
            g.type = static_cast<uint32_t>(type);
            g.castShadow = castShadow ? 1.0f : 0.0f;
            return g;
        }

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", name);
            node.SetAttribute("type", (int)type);
            node.SetAttribute("intensity", intensity);
            node.SetAttribute("radius", radius);
            node.SetAttribute("innerConeAngleDeg", innerConeAngleDeg);
            node.SetAttribute("outerConeAngleDeg", outerConeAngleDeg);
            node.SetAttribute("castShadow", castShadow);

            XMLNode posNode = node.AddChild("position");
            posNode.SetAttribute("x", position.x);
            posNode.SetAttribute("y", position.y);
            posNode.SetAttribute("z", position.z);

            XMLNode dirNode = node.AddChild("direction");
            dirNode.SetAttribute("x", direction.x);
            dirNode.SetAttribute("y", direction.y);
            dirNode.SetAttribute("z", direction.z);

            XMLNode colNode = node.AddChild("color");
            colNode.SetAttribute("r", color.r);
            colNode.SetAttribute("g", color.g);
            colNode.SetAttribute("b", color.b);
        }

        void Deserialize(const XMLNode &node) override
        {
            node.GetAttribute("name", name);
            int t = 0;
            node.GetAttribute("type", t);
            type = (Lighting::LightType)t;
            node.GetAttribute("intensity", intensity);
            node.GetAttribute("radius", radius);
            node.GetAttribute("innerConeAngleDeg", innerConeAngleDeg);
            node.GetAttribute("outerConeAngleDeg", outerConeAngleDeg);
            node.GetAttribute("castShadow", castShadow);

            XMLNode posNode = node.GetChild("position");
            if (posNode.IsValid())
            {
                posNode.GetAttribute("x", position.x);
                posNode.GetAttribute("y", position.y);
                posNode.GetAttribute("z", position.z);
            }

            XMLNode dirNode = node.GetChild("direction");
            if (dirNode.IsValid())
            {
                dirNode.GetAttribute("x", direction.x);
                dirNode.GetAttribute("y", direction.y);
                dirNode.GetAttribute("z", direction.z);
            }

            XMLNode colNode = node.GetChild("color");
            if (colNode.IsValid())
            {
                colNode.GetAttribute("r", color.r);
                colNode.GetAttribute("g", color.g);
                colNode.GetAttribute("b", color.b);
            }
        }
    };
}
