#pragma once

#include "Matrix/Matrix2.hpp"
#include "Matrix/Matrix3.hpp"
#include "Matrix/Matrix4.hpp"
#include "Quaternion/Quaternion.hpp"
#include "Vectors/Vector.hpp"

// so we dont have to put using namespace glm; everywhere
namespace SF::Engine
{
    using glm::cross;
    using glm::inverse;
    using glm::inversesqrt;
    using glm::normalize;

    using glm::max;
    using glm::min;

    using glm::double2x2;
    using glm::double2x3;
    using glm::double2x4;
    using glm::double3x2;
    using glm::double3x3;
    using glm::double3x4;
    using glm::double4x2;
    using glm::double4x3;
    using glm::double4x4;

    using glm::bool2;
    using glm::bool3;
    using glm::bool4;
    using glm::double2;
    using glm::double3;
    using glm::double4;
    using glm::float2;
    using glm::float3;
    using glm::float4;
    using glm::int2;
    using glm::int3;
    using glm::int4;

    using glm::int2x2;
    using glm::int2x3;
    using glm::int2x4;
    using glm::int3x2;
    using glm::int3x3;
    using glm::int3x4;
    using glm::int4x2;
    using glm::int4x3;
    using glm::int4x4;

    using glm::float2x2;
    using glm::float2x3;
    using glm::float2x4;
    using glm::float3x2;
    using glm::float3x3;
    using glm::float3x4;
    using glm::float4x2;
    using glm::float4x3;
    using glm::float4x4;

    using glm::bool2x2;
    using glm::bool2x3;
    using glm::bool2x4;
    using glm::bool3x2;
    using glm::bool3x3;
    using glm::bool3x4;
    using glm::bool4x2;
    using glm::bool4x3;
    using glm::bool4x4;
} // namespace SF::Engine
