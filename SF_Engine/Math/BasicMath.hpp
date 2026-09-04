#pragma once

#include <1stPartyLibs/TemplateLibrary/TypeTraits.hpp>
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

}