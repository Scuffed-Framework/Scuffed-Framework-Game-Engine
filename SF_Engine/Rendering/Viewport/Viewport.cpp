#include "Viewport.hpp"

namespace SF::Engine
{
    SceneViewport::SceneViewport(UVec2 extent)
        : desiredExtent(extent), pendingFree(kFramesInFlight)
    {
        CreateImages(extent);
    }

    SceneViewport::~SceneViewport()
    {
        if (imguiDescriptor != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(imguiDescriptor);

        for (auto &pending : pendingFree)
            if (pending) ImGui_ImplVulkan_RemoveTexture(*pending);
    }

    void SceneViewport::CreateImages(UVec2 extent)
    {
        currentExtent = extent;

        color = std::make_unique<Image2d>(UVec2{extent.x, extent.y},
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLE_COUNT_1_BIT, 1, 1);

        depth = std::make_unique<ImageDepth>(extent, VK_SAMPLE_COUNT_1_BIT);

        // ImGui descriptor NOT created here anymore -- deferred to
        // GetImGuiTexture(), since ImGui_ImplVulkan_Init() may not have run yet
        // when this fires from the Application constructor.
        imguiDescriptor = VK_NULL_HANDLE;
        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkDescriptorSet SceneViewport::GetImGuiTexture()
    {
        if (imguiDescriptor == VK_NULL_HANDLE)
        {
            imguiDescriptor = ImGui_ImplVulkan_AddTexture(
                color->GetSampler(), color->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        return imguiDescriptor;
    }

    void SceneViewport::SetDesiredExtent(UVec2 extent)
    {
        if (extent.x == 0 || extent.y == 0) return;
        if (extent.x == desiredExtent.x && extent.y == desiredExtent.y) return;
        desiredExtent = extent;
        resizePending = true;
    }

    void SceneViewport::Tick(std::size_t frameIndex)
    {
        if (!resizePending) return;
        resizePending = false;

        std::size_t safeIndex = frameIndex % kFramesInFlight;

        if (auto &pending = pendingFree[safeIndex])
        {
            ImGui_ImplVulkan_RemoveTexture(*pending);
            pending.reset();
        }

        if (imguiDescriptor != VK_NULL_HANDLE)
            pendingFree[safeIndex] = imguiDescriptor;

        CreateImages(desiredExtent);
    }

    void SceneViewport::PrepareForRender(VkCommandBuffer cmd)
    {
        if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) return;

        VkImageLayout old = currentLayout;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = old;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = color->GetImage();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = (old == VK_IMAGE_LAYOUT_UNDEFINED) ? 0 : VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkPipelineStageFlags srcStage = (old == VK_IMAGE_LAYOUT_UNDEFINED)
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &barrier);
        currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    void SceneViewport::BeginRendering(VkCommandBuffer cmd, VkClearColorValue clear)
    {
        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = color->GetView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = clear;

        VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAttachment.imageView = depth->GetView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkExtent2D extent{currentExtent.x, currentExtent.y};

        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        renderingInfo.renderArea = {{0, 0}, extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport vp{0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    void SceneViewport::EndRendering(VkCommandBuffer cmd)
    {
        vkCmdEndRendering(cmd);
    }

    void SceneViewport::PrepareForSample(VkCommandBuffer cmd)
    {
        if (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return;

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = currentLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = color->GetImage();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &barrier);
        currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}