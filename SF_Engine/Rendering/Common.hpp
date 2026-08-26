#pragma once
#include "RenderSystem.hpp"
namespace SF::Engine
{
    inline VkDescriptorSetLayout CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &createInfo)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        const auto device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        const VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout);
        RenderSystem::RenderSystem::CheckVkResult(result);

        return layout;
    }

    // Create descriptor pool
    inline VkDescriptorPool CreateDescriptorPool(const VkDescriptorPoolCreateInfo &createInfo)
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        auto device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        VkResult result = vkCreateDescriptorPool(device, &createInfo, nullptr, &pool);
        RenderSystem::RenderSystem::CheckVkResult(result);

        return pool;
    }

    // Destroy descriptor set layout
    inline void DestroyDescriptorSetLayout(VkDescriptorSetLayout layout)
    {
        if (layout != VK_NULL_HANDLE)
        {
            auto device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    }

    // Destroy descriptor pool
    inline void DestroyDescriptorPool(VkDescriptorPool pool)
    {
        if (pool != VK_NULL_HANDLE)
        {
            auto device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
    }
}