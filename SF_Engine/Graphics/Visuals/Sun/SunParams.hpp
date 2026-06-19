#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace SF::Engine
{
    // std140 layout : 208 bytes, verified % 16 == 0.
    //   mat4  invProj          offset=0    size=64
    //   mat4  invView          offset=64   size=64
    //   vec4  sunDir           offset=128  size=16  (.w=intensity)
    //   vec4  sunColor         offset=144  size=16  (.w=unused)
    //   vec2  screenSize       offset=160  size=8
    //   float discHalfAngleCos offset=168  size=4
    //   float haloHalfAngleCos offset=172  size=4
    //   float haloStrength     offset=176  size=4
    //   float bloomStrength    offset=180  size=4
    //   vec2  _pad             offset=184  size=8
    //   total = 192 bytes
    struct alignas(16) SunUBO
    {
        glm::mat4 invProj;      // 0
        glm::mat4 invView;      // 64
        glm::vec4 sunDir;       // 128  .w = intensity
        glm::vec4 sunColor;     // 144  .w = unused
        glm::vec2 screenSize;   // 160
        float discHalfAngleCos; // 168  cos(disc angular radius)
        float haloHalfAngleCos; // 172  cos(halo angular radius)
        float haloStrength;     // 176
        float bloomStrength;    // 180  extra HDR push for bloom threshold
        glm::vec2 _pad;         // 184
    }; // total 192
    static_assert(sizeof(SunUBO) % 16 == 0);

    struct SunParams
    {
        glm::vec3 color = {1.0f, 0.95f, 0.85f}; // warm white
        float intensity = 20.0f;
        float discAngleDeg = 0.27f;
        float haloAngleDeg = 4.0f;
        float haloStrength = 0.35f;
        float bloomStrength = 6.0f;
    };
}
