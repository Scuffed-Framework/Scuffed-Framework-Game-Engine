#pragma once

#include "LightManager.hpp"
#include "LitMeshPipelinePass.hpp"
#include "GBufferPass.hpp"
#include "ClusterCullPipelinePass.hpp"
#include "DeferredLightPipelinePass.hpp"
#include "ForwardTransparentPipelinePass.hpp"

#include <Scene/Types.hpp>

namespace SF::Engine
{
    inline SceneLight &MakeLight(
        std::vector<SceneLight> &lights,
        const char *name,
        Lighting::LightType type,
        glm::vec3 color,
        float intensity,
        glm::vec3 pos,
        glm::vec3 rotDeg,
        float radius = 10.0f)
    {
        SceneLight &sl = lights.emplace_back();

        sl.name = name;
        sl.light.name = name;
        sl.light.type = type;
        sl.light.color = color;
        sl.light.intensity = intensity;
        sl.light.radius = radius;
        sl.light.castShadow = true;

        sl.transform.position = pos;
        sl.transform.rotation = rotDeg;

        if (type == Lighting::LightType::Directional)
        {
            glm::vec3 rot = glm::radians(rotDeg);
            glm::mat4 m = glm::rotate(glm::mat4(1.0f), rot.y, {0, 1, 0});
            m = glm::rotate(m, rot.x, {1, 0, 0});
            m = glm::rotate(m, rot.z, {0, 0, 1});
            sl.light.direction = glm::normalize(glm::vec3(m * glm::vec4(0, -1, 0, 0)));
        }

        return sl;
    }
}