#pragma once
#include <cstdint>

#include <Math/Vectors/Vector2.hpp>
#include <volk.h>

namespace SF::Engine
{
    enum class ResourceUsage
    {
        None,
        ColorAttachment,
        DepthStencilAttachment,
        SampledTexture, // Shader Read
        StorageWrite    // Compute Write
    };

    struct Texture
    {
        VkImage image    = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format;
        VkImageAspectFlags aspect_mask;
    };

    struct ResourceState
    {
        VkImageLayout current_layout     = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 last_stage = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 last_access       = VK_ACCESS_2_NONE;
    };

    using VulkanStateMapping = ResourceState;

    inline VulkanStateMapping GetVulkanState(ResourceUsage usage)
    {
        switch (usage)
        {
            case ResourceUsage::ColorAttachment:
                return {.current_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .last_stage     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .last_access    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
            case ResourceUsage::DepthStencilAttachment:
                return {.current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .last_stage     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        .last_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
            case ResourceUsage::SampledTexture:
                return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT};
            default:
                return {.current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .last_stage     = VK_PIPELINE_STAGE_2_NONE,
                        .last_access    = VK_ACCESS_2_NONE};
        }
    }

    using namespace std;
    class FrameGraph
    {
    };

} // namespace SF::Engine
