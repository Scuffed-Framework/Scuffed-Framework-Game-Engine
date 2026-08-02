#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace SF::Engine
{
    /**
     * @brief some global samplers
     *
     */
    class SharedSamplers
    {
    public:
        static void CreateSamplers();
        static void DestroySamplers();

        static VkSampler GetLinearClampSampler() { return linearClampSampler_; }
        static VkSampler GetLinearRepeatSampler() { return linearRepeatSampler_; }
        static VkSampler GetNearestClampSampler() { return nearestClampSampler_; }
        static VkSampler GetNearestRepeatSampler() { return nearestRepeatSampler_; }

    private:
        static VkSampler linearClampSampler_;
        static VkSampler linearRepeatSampler_;
        static VkSampler nearestClampSampler_;
        static VkSampler nearestRepeatSampler_;
    };
}