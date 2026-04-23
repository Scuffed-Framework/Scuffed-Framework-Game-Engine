#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace SF::Engine::Lighting
{
    static constexpr uint32_t MAX_LIGHTS = 4096;
    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 128;
    static constexpr uint32_t CLUSTER_X = 16;
    static constexpr uint32_t CLUSTER_Y = 9;
    static constexpr uint32_t CLUSTER_Z = 24;
    static constexpr uint32_t CLUSTER_COUNT = CLUSTER_X * CLUSTER_Y * CLUSTER_Z;

    enum class LightType : uint32_t
    {
        Point = 0,
        Spot = 1,
        Directional = 2,
    };

    // std430, 64 bytes exactly
    struct alignas(16) GpuLight
    {
        glm::vec3 position;
        float radius;

        glm::vec3 color;
        float intensity;

        glm::vec3 direction;
        float innerConeAngle; // cos(inner half-angle), spot only

        float outerConeAngle; // cos(outer half-angle), spot only
        uint32_t type;        // LightType cast to uint
        float castShadow;     // 1 = yes
        float _pad;
    };
    static_assert(sizeof(GpuLight) == 64);

    // Cluster AABB (view-space)
    struct GpuCluster
    {
        glm::vec4 minAABB;
        glm::vec4 maxAABB;
    };

    // Per-cluster light list header
    struct GpuClusterLightList
    {
        uint32_t offset;
        uint32_t count;
    };

    // std140 : must be multiple of 16 bytes
    struct alignas(16) GpuFrameData
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
        glm::mat4 invView;
        glm::mat4 invProj;
        glm::mat4 invViewProj;

        glm::vec4 cameraPos; // w = near
        glm::vec4 cameraDir; // w = far
        glm::vec2 screenSize;
        glm::vec2 invScreenSize;

        float nearPlane;
        float farPlane;
        float time;
        float deltaTime;

        uint32_t lightCount;
        uint32_t frameIndex;
        glm::vec2 _pad;

        // Sun: world-space direction toward the sun (unit vector), w = sunIntensity.
        // Filled by the renderer each frame alongside the directional light.
        // Used by Lit.shader to modulate the ambient term so the scene goes dark
        // when the sun dips below the horizon (matching the atmosphere response).
        glm::vec4 sunDirIntensity; // .xyz = towardSun, .w = intensity (0 at night)
    };
    static_assert(sizeof(GpuFrameData) % 16 == 0);
}
