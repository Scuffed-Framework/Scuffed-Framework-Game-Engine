#pragma once

#include "Matrix/Matrix2.hpp"
#include "Matrix/Matrix3.hpp"
#include "Matrix/Matrix4.hpp"
#include "Vectors/Vector.hpp"
#include "Quaternion/Quaternion.hpp"
#include <TemplateLibrary/TypeTraits.hpp>

// so we dont have to put using namespace glm; everywhere
namespace SF::Engine
{
    using glm::normalize;
    using glm::inverse;
    using glm::inversesqrt;
    using glm::cross;

    using glm::min;
    using glm::max;

}