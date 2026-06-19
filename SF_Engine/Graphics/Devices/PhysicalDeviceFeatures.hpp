#pragma once

#define VK_NO_PROTOTYPES
#include <volk.h>
#include "PhysicalDevice.hpp" // includes volk
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    class PhysicalDeviceFeatures
    {
    public:
        bool Supports64BitAtomics()
        {
            VkPhysicalDeviceVulkan12Features deviceFeatures12{};
            deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            deviceFeatures12.pNext = nullptr;

            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &deviceFeatures12;

            vkGetPhysicalDeviceFeatures(
                RenderSystem::Get()->GetPhysicalDevice()->GetPhysicalDevice(),
                &deviceFeatures2);

            return (deviceFeatures12.shaderBufferInt64Atomics == VK_TRUE &&
                    deviceFeatures12.shaderSharedInt64Atomics == VK_TRUE);
        }
    };
}