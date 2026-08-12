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
        Vec3 color,
        float intensity,
        Vec3 pos,
        Vec3 rotDeg,
        float radius = 10.0f)
    {
        SceneLight &sl = lights.emplace_back(name);

        sl.GetComponent<Light>()->name = name;
        sl.GetComponent<Light>()->type = type;
        sl.GetComponent<Light>()->color = color;
        sl.GetComponent<Light>()->intensity = intensity;
        sl.GetComponent<Light>()->radius = radius;
        sl.GetComponent<Light>()->castShadow = true;

        sl.GetComponent<Transform>()->position = pos;
        sl.GetComponent<Transform>()->rotation = rotDeg;

        if (type == Lighting::LightType::Directional)
        {
            Vec3 rot = glm::radians(rotDeg);
            Mat4 m = glm::rotate(Mat4(1.0f), rot.y, {0, 1, 0});
            m = glm::rotate(m, rot.x, {1, 0, 0});
            m = glm::rotate(m, rot.z, {0, 0, 1});
            sl.GetComponent<Light>()->direction = normalize(Vec3(m * Vec4(0, -1, 0, 0)));
        }

        return sl;
    }
}