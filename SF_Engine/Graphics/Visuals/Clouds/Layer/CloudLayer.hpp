#pragma once
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <glm/glm.hpp>
#include <Math/KVP.hpp>
#include <algorithm>

namespace SF::Engine
{
    struct CloudLayer
    {
        DescriptorSet desc;
        float BottomRadius;
        float TopRadius;

        float MaxShadowAltitude;
        float AverageNoiseScale;

        glm::vec3 noiseScroll;
        glm::vec4 worldToCloud;
        glm::vec4 prevWorldToCloud;
        glm::vec4 GetWorldToCloud()
        {
            return worldToCloud;
        }

        glm::vec3 GetNoiseScroll()
        {
            return noiseScroll;
        }
        static double ApplyErosion(double highDetail, double lowDetail, double erosionMaxDepth, double edgeSharpness)
        {
            highDetail = highDetail * erosionMaxDepth + (1.0 - erosionMaxDepth);
            double num = 1.0 - lowDetail;
            double num2 = highDetail - num;
            double res = std::clamp(num2 / (1.0 - edgeSharpness), 0.0, 1.0);
            return res;
        }
    };

}