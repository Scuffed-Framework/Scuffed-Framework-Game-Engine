#include "SharedSamplers.hpp"
#include "RenderSystem.hpp"
#include <stdexcept>

namespace SF::Engine
{
    VkSampler SharedSamplers::linearClampSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearRepeatSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::nearestClampSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::nearestRepeatSampler_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::pointClampEdge_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::pointClampBorder0000_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::pointRepeat_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearClampBorder0000_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearClampBorder1111_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::pointClampBorder1111_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearClampEdgeMipFilter_ = VK_NULL_HANDLE;
    VkSampler SharedSamplers::linearRepeatMipFilter_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout SharedSamplers::sharedSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool SharedSamplers::sharedSetPool_ = VK_NULL_HANDLE;
    VkDescriptorSet SharedSamplers::sharedSet_ = VK_NULL_HANDLE;

    void SharedSamplers::CreateSamplers()
    {
        VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        auto makeSampler = [&](VkFilter filter, VkSamplerAddressMode mode, VkBorderColor border,
                               bool mipFilter) -> VkSampler
        {
            VkSamplerCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = filter;
            info.minFilter = filter;
            info.addressModeU = mode;
            info.addressModeV = mode;
            info.addressModeW = mode;
            info.anisotropyEnable = VK_FALSE;
            info.maxAnisotropy = 1.0f;
            info.borderColor = border;
            info.unnormalizedCoordinates = VK_FALSE;
            info.compareEnable = VK_FALSE;
            info.compareOp = VK_COMPARE_OP_ALWAYS;
            info.mipmapMode = mipFilter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
            info.minLod = 0.0f;
            info.maxLod = mipFilter ? VK_LOD_CLAMP_NONE : 0.0f;

            VkSampler s = VK_NULL_HANDLE;
            if (vkCreateSampler(device, &info, nullptr, &s) != VK_SUCCESS)
                throw std::runtime_error("Failed to create shared sampler");
            return s;
        };

        try
        {
            linearClampSampler_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_INT_OPAQUE_BLACK, false);
            linearRepeatSampler_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_INT_OPAQUE_BLACK, false);
            nearestClampSampler_ = makeSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_INT_OPAQUE_BLACK, false);
            nearestRepeatSampler_ = makeSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_INT_OPAQUE_BLACK, false);

            // Bindings 0-9 in Samplers.si, in exact order:
            pointClampEdge_ = nearestClampSampler_;                                                                                                          // binding 0 — reuse, same params
            pointClampBorder0000_ = makeSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, false); // binding 1
            pointRepeat_ = nearestRepeatSampler_;                                                                                                            // binding 2 — reuse
            linearClampBorder0000_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, false); // binding 4
            linearClampBorder1111_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, false);      // binding 6
            pointClampBorder1111_ = makeSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, false);      // binding 7
            linearClampEdgeMipFilter_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_INT_OPAQUE_BLACK, true);        // binding 8
            linearRepeatMipFilter_ = makeSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_INT_OPAQUE_BLACK, true);                  // binding 9
        }
        catch (...)
        {
            DestroySamplers();
            throw;
        }

        CreateSharedSamplerDescriptorSet();
    }

    void SharedSamplers::CreateSharedSamplerDescriptorSet()
    {
        VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        // Layout: 10 bindings, all VK_DESCRIPTOR_TYPE_SAMPLER, matching Samplers.si order exactly.
        VkDescriptorSetLayoutBinding bindings[10]{};
        for (uint32_t i = 0; i < 10; ++i)
        {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_ALL; // used from compute + fragment shaders alike
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 10;
        layoutInfo.pBindings = bindings;
        RenderSystem::CheckVkResult(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &sharedSetLayout_));

        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 10};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        RenderSystem::CheckVkResult(vkCreateDescriptorPool(device, &poolInfo, nullptr, &sharedSetPool_));

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = sharedSetPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &sharedSetLayout_;
        RenderSystem::CheckVkResult(vkAllocateDescriptorSets(device, &allocInfo, &sharedSet_));

        VkSampler samplersInOrder[10] = {
            pointClampEdge_, pointClampBorder0000_, pointRepeat_, linearClampSampler_,
            linearClampBorder0000_, linearRepeatSampler_, linearClampBorder1111_,
            pointClampBorder1111_, linearClampEdgeMipFilter_, linearRepeatMipFilter_};

        VkDescriptorImageInfo imageInfos[10]{};
        VkWriteDescriptorSet writes[10]{};
        for (uint32_t i = 0; i < 10; ++i)
        {
            imageInfos[i].sampler = samplersInOrder[i];
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = sharedSet_;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            writes[i].pImageInfo = &imageInfos[i];
        }
        vkUpdateDescriptorSets(device, 10, writes, 0, nullptr);
    }

    void SharedSamplers::BindSharedSamplerSet(const CommandBuffer &cmd, VkPipelineLayout pipelineLayout,
                                              VkPipelineBindPoint bindPoint, uint32_t setIndex)
    {
        assert(cmd.GetCommandBuffer() != VK_NULL_HANDLE);
        assert(pipelineLayout != VK_NULL_HANDLE);
        assert(sharedSet_ != VK_NULL_HANDLE);

        vkCmdBindDescriptorSets(cmd.GetCommandBuffer(), bindPoint, pipelineLayout, setIndex, 1, &sharedSet_, 0, nullptr);
    }

    void SharedSamplers::DestroySamplers()
    {
        VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice();

        vkDestroyDescriptorPool(device, sharedSetPool_, nullptr); // also frees sharedSet_
        vkDestroyDescriptorSetLayout(device, sharedSetLayout_, nullptr);
        sharedSetPool_ = VK_NULL_HANDLE;
        sharedSetLayout_ = VK_NULL_HANDLE;
        sharedSet_ = VK_NULL_HANDLE;

        vkDestroySampler(device, linearClampSampler_, nullptr);
        vkDestroySampler(device, linearRepeatSampler_, nullptr);
        vkDestroySampler(device, nearestClampSampler_, nullptr);
        vkDestroySampler(device, nearestRepeatSampler_, nullptr);
        vkDestroySampler(device, pointClampBorder0000_, nullptr);
        vkDestroySampler(device, linearClampBorder0000_, nullptr);
        vkDestroySampler(device, linearClampBorder1111_, nullptr);
        vkDestroySampler(device, pointClampBorder1111_, nullptr);
        vkDestroySampler(device, linearClampEdgeMipFilter_, nullptr);
        vkDestroySampler(device, linearRepeatMipFilter_, nullptr);
        // pointClampEdge_/pointRepeat_ intentionally not destroyed twice — they alias
        // nearestClampSampler_/nearestRepeatSampler_.

        linearClampSampler_ = linearRepeatSampler_ = nearestClampSampler_ = nearestRepeatSampler_ = VK_NULL_HANDLE;
        pointClampEdge_ = pointClampBorder0000_ = pointRepeat_ = VK_NULL_HANDLE;
        linearClampBorder0000_ = linearClampBorder1111_ = pointClampBorder1111_ = VK_NULL_HANDLE;
        linearClampEdgeMipFilter_ = linearRepeatMipFilter_ = VK_NULL_HANDLE;
    }
}