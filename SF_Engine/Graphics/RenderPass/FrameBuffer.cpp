#include "Framebuffer.hpp"

#include <Graphics/Images/ImageDepth.hpp>
#include <Graphics/Renderpass/Renderpass.hpp>
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Stage.hpp>

namespace SF::Engine
{
    Framebuffer::Framebuffer(const LogicalDevice &logicalDevice, const Swapchain &swapchain, const RenderStage &renderStage, const Renderpass &renderPass, const ImageDepth &depthStencil,
                             const Vector2Uint &extent, VkSampleCountFlagBits samples) : logicalDevice(logicalDevice)
    {
        for (const auto &attachment : renderStage.GetAttachments())
        {
            auto attachmentSamples = attachment.IsMultisampled() ? samples : VK_SAMPLE_COUNT_1_BIT;

            switch (attachment.GetType())
            {
            case Attachment::Type::Image:
                imageAttachments.emplace_back(std::make_unique<Image2d>(extent, attachment.GetFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, attachmentSamples));
                break;
            case Attachment::Type::Depth:
                imageAttachments.emplace_back(nullptr);
                break;
            case Attachment::Type::Swapchain:
                imageAttachments.emplace_back(nullptr);
                break;
            }
        }

        framebuffer.resize(swapchain.GetImageCount());

        for (uint32_t i = 0; i < swapchain.GetImageCount(); i++)
        {
            std::vector<VkImageView> attachments;

            for (const auto &attachment : renderStage.GetAttachments())
            {
                switch (attachment.GetType())
                {
                case Attachment::Type::Image:
                    attachments.emplace_back(GetAttachment(attachment.GetBinding())->GetView());
                    break;
                case Attachment::Type::Depth:
                    attachments.emplace_back(depthStencil.GetView());
                    break;
                case Attachment::Type::Swapchain:
                    attachments.emplace_back(swapchain.GetImageViews().at(i));
                    break;
                }
            }

            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.renderPass = renderPass;
            framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferCreateInfo.pAttachments = attachments.data();
            framebufferCreateInfo.width = extent.x;
            framebufferCreateInfo.height = extent.y;
            framebufferCreateInfo.layers = 1;
            RenderSystem::CheckVkResult(vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &framebuffer[i]));
        }
    }

    Framebuffer::~Framebuffer()
    {
        for (const auto &framebuffer : framebuffer)
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }
}