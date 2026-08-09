#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace SF::Engine
{
    class DescriptorSet;
    class CommandBuffer;

    class SharedSamplers
    {
    public:
        static void CreateSamplers();
        static void DestroySamplers();

        static VkSampler GetLinearClampSampler() { return linearClampSampler_; }
        static VkSampler GetLinearRepeatSampler() { return linearRepeatSampler_; }
        static VkSampler GetNearestClampSampler() { return nearestClampSampler_; }
        static VkSampler GetNearestRepeatSampler() { return nearestRepeatSampler_; }

        static const VkDescriptorSetLayout &GetSharedSamplerSetLayout() { return sharedSetLayout_; }
        static void BindSharedSamplerSet(const CommandBuffer &cmd, VkPipelineLayout pipelineLayout,
                                          VkPipelineBindPoint bindPoint, uint32_t setIndex = 1);

    private:
        static void CreateSharedSamplerDescriptorSet();

        static VkSampler linearClampSampler_;
        static VkSampler linearRepeatSampler_;
        static VkSampler nearestClampSampler_;
        static VkSampler nearestRepeatSampler_;

        // Matches Samplers.si bindings 0-9 exactly.
        static VkSampler pointClampEdge_;
        static VkSampler pointClampBorder0000_;
        static VkSampler pointRepeat_;
        static VkSampler linearClampBorder0000_;
        static VkSampler linearClampBorder1111_;
        static VkSampler pointClampBorder1111_;
        static VkSampler linearClampEdgeMipFilter_;
        static VkSampler linearRepeatMipFilter_;

        static VkDescriptorSetLayout sharedSetLayout_;
        static VkDescriptorPool sharedSetPool_;
        static VkDescriptorSet sharedSet_;
    };
}