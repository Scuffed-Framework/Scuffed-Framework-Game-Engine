#pragma once
#include <Math/BasicMath.hpp>

namespace SF::Engine
{
    //  Tunable atmosphere parameters
    struct AtmosphereParams
    {
        float bottomRadius = 6'371'000.0f;     // Planet ground radius (m)
        float topRadius = 6'471'000.0f;        // Atmosphere top radius (m)
        float sunIntensity = 20.0f;            // idk
        float renderUnitRadius = 6'371'000.0f; // Engine units that equal bottomRadius metres.
                                               // Default: 1 engine unit = 1 meter.
    };

    //  UBO sent to Atmosphere.shader every frame
    // std140 layout : must match the uniform block in Atmosphere.shader exactly.
    struct alignas(16) AtmosphereFrameUBO
    {
        Mat4 invProj;
        Mat4 invView;
        Vec4 cameraPos;    // .xyz = viewPos in metres (camera - planet centre)
        Vec4 planetPos;    // .xyz = planet centre (always vec3(0) in shader convention)
        Vec4 sunDir;       // .xyz = toward sun (unit vector), .w = sunIntensity
        float bottomRadius;     // metres
        float topRadius;        // metres
        float renderUnitRadius; // unused by shader, kept for UBO size alignment
        glm::vec2 screenSize;
        Vec3 sunCol;
    };
    static_assert(sizeof(AtmosphereFrameUBO) % 16 == 0);

    struct AtmosphereData
    {
        AtmosphereParams params;
        AtmosphereFrameUBO ubo;
    };

}
