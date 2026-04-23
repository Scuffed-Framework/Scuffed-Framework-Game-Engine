#pragma once

#include <Graphics/Images/Image2d.hpp>
#include "Swapchain.hpp"

namespace SF::Engine
{
    class LogicalDevice;
    class ImageDepth;
    class Renderpass;
    class RenderStage;

    class Framebuffer : NoCopy
    {
    public:
        Framebuffer(const LogicalDevice &logicalDevice, const Swapchain &swapchain, const RenderStage &renderStage, const Renderpass &renderPass, const ImageDepth &depthStencil,
                    const Vector2Uint &extent, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
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