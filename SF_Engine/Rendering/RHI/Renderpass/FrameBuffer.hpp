#pragma once

#include <Rendering/RHI/Images/Image2d.hpp>
#include "RhiSwapchain.hpp"

namespace SF::Engine
{
    class LogicalDevice;
    class ImageDepth;
    class RhiRenderpass;
    class RhiRenderStage;

    class Framebuffer : NoCopy
    {
    public:
        Framebuffer(const LogicalDevice &logicalDevice, const RhiSwapchain &swapchain, const RhiRenderStage &renderStage,
                    const RhiRenderpass &renderPass, const ImageDepth *depthStencil, // pointer now
                    const UVec2 &extent, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
        ~Framebuffer();

        Image2d *GetAttachment(uint32_t index) const { return imageAttachments[index].get(); }

        const std::vector<std::unique_ptr<Image2d>> &GetImageAttachments() const { return imageAttachments; }
        const std::vector<VkFramebuffer> &GetFramebuffer() const { return framebuffer; }

    private:
        const LogicalDevice &logicalDevice;

        std::vector<std::unique_ptr<Image2d>> imageAttachments;
        std::vector<VkFramebuffer> framebuffer;
    };
}