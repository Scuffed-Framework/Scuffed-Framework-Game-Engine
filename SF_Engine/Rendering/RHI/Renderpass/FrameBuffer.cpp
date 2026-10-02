#include "FrameBuffer.hpp"

#include <Rendering/FrameGraph/Stage.hpp>
#include <Rendering/RenderSystem.hpp>
#include "RhiRenderpass.hpp"

namespace SF::Engine
{
    Framebuffer::Framebuffer(const LogicalDevice &logicalDevice, const RhiSwapchain &swapchain,
                             const RhiRenderStage &renderStage, const RhiRenderpass &renderPass,
                             const ImageDepth *depthStencil, const UVec2 &extent, VkSampleCountFlagBits samples) :
        logicalDevice(logicalDevice)
    {
        for (const auto &attachment: renderStage.GetAttachments())
        {
            auto attachmentSamples = attachment.IsMultisampled() ? samples : VK_SAMPLE_COUNT_1_BIT;

            switch (attachment.GetType())
            {
                case RhiAttachment::Type::Image:
                    // Created in SHADER_READ_ONLY_OPTIMAL (the renderpass finalLayout), not
                    // COLOR_ATTACHMENT_OPTIMAL: passes that sample an attachment in PreRender (SSR
                    // reads "hdr" before its stage's renderpass has run) assume SHADER_READ_ONLY,
                    // which on the first frame after a rebuild was a layout mismatch. The renderpass
                    // itself uses initialLayout=UNDEFINED, so this is safe for rendering.
                    imageAttachments.emplace_back(std::make_unique<Image2d>(
                            extent, attachment.GetFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_FILTER_LINEAR,
                            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, attachmentSamples));
                    break;
                case RhiAttachment::Type::Depth:
                    imageAttachments.emplace_back(nullptr);
                    break;
                case RhiAttachment::Type::Swapchain:
                    imageAttachments.emplace_back(nullptr);
                    break;
            }
        }

        framebuffer.resize(swapchain.GetImageCount());

        for (uint32_t i = 0; i < swapchain.GetImageCount(); i++)
        {
            std::vector<VkImageView> attachments;

            for (const auto &attachment: renderStage.GetAttachments())
            {
                switch (attachment.GetType())
                {
                    case RhiAttachment::Type::Image:
                        attachments.emplace_back(GetAttachment(attachment.GetBinding())->GetView());
                        break;
                    case RhiAttachment::Type::Depth:
                        // Only reached when this stage actually declares a Depth
                        // attachment, so depthStencil is guaranteed non-null here.
                        attachments.emplace_back(depthStencil->GetView());
                        break;
                    case RhiAttachment::Type::Swapchain:
                        attachments.emplace_back(swapchain.GetImageViews().at(i));
                        break;
                }
            }

            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.renderPass              = renderPass;
            framebufferCreateInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
            framebufferCreateInfo.pAttachments            = attachments.data();
            framebufferCreateInfo.width                   = extent.x;
            framebufferCreateInfo.height                  = extent.y;
            framebufferCreateInfo.layers                  = 1;
            RenderSystem::CheckVkResult(
                    vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &framebuffer[i]));
        }
    }

    Framebuffer::~Framebuffer()
    {
        for (const auto &framebuffer: framebuffer)
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }
}