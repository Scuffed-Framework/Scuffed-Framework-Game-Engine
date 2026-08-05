#include "SharedSamplers.hpp"
#include "RenderSystem.hpp"
#include <stdexcept>

namespace SF::Engine
{
    VkSampler SharedSamplers::linearClampSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearRepeatSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::nearestClampSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::nearestRepeatSampler_ = VK_NULL_HANDLE;

    void SharedSamplers::CreateSamplers()
    {
        VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        try
        {
            if (vkCreateSampler(device, &samplerInfo, nullptr, &linearClampSampler_) != VK_SUCCESS)
                throw std::runtime_error("Failed to create linear clamp sampler");

            // Repeat Sampler
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (vkCreateSampler(device, &samplerInfo, nullptr, &linearRepeatSampler_) != VK_SUCCESS)
                throw std::runtime_error("Failed to create linear repeat sampler");

            // Nearest Clamp Sampler
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            if (vkCreateSampler(device, &samplerInfo, nullptr, &nearestClampSampler_) != VK_SUCCESS)
                throw std::runtime_error("Failed to create nearest clamp sampler");

            // Nearest Repeat Sampler
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (vkCreateSampler(device, &samplerInfo, nullptr, &nearestRepeatSampler_) != VK_SUCCESS)
                throw std::runtime_error("Failed to create nearest repeat sampler");
        }
        catch (...)
        {
            // Don't leak whichever samplers succeeded before the failure
            DestroySamplers();
            throw /*the game*/;
        }
    }

    void SharedSamplers::DestroySamplers()
    {
        VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();
        vkDestroySampler(device, linearClampSampler_, nullptr);
        vkDestroySampler(device, linearRepeatSampler_, nullptr);
        vkDestroySampler(device, nearestClampSampler_, nullptr);
        vkDestroySampler(device, nearestRepeatSampler_, nullptr);

        linearClampSampler_ = VK_NULL_HANDLE;
        linearRepeatSampler_ = VK_NULL_HANDLE;
        nearestClampSampler_ = VK_NULL_HANDLE;
        nearestRepeatSampler_ = VK_NULL_HANDLE;
    }
}