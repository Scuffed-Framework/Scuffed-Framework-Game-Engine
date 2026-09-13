#include "RhiRenderpass.hpp"

#include <Rendering/FrameGraph/Stage.hpp>
#include <Rendering/RenderSystem.hpp>

namespace SF::Engine
{
    RhiRenderpass::RhiRenderpass(const LogicalDevice &logicalDevice, const RhiRenderStage &renderStage,
                                 VkFormat depthFormat, VkFormat surfaceFormat, VkSampleCountFlagBits samples) :
        logicalDevice(logicalDevice)
    {
        // Creates the renderpasses attachment descriptions,
        std::vector<VkAttachmentDescription> attachmentDescriptions;

        for (const auto &attachment: renderStage.GetAttachments())
        {
            auto attachmentSamples = attachment.IsMultisampled() ? samples : VK_SAMPLE_COUNT_1_BIT;

            VkAttachmentDescription attachmentDescription = {};
            attachmentDescription.samples                 = attachmentSamples;
            attachmentDescription.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear at beginning of the render pass.
            attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // // The image can be read from so it's
                                                                          // important to store the attachment results
            attachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescription.initialLayout =
                    VK_IMAGE_LAYOUT_UNDEFINED; // We don't care about initial layout of the attachment.

            switch (attachment.GetType())
            {
                case RhiAttachment::Type::Image:
                    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    attachmentDescription.format      = attachment.GetFormat();
                    break;
                case RhiAttachment::Type::Depth:
                    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    attachmentDescription.format      = depthFormat;
                    break;
                case RhiAttachment::Type::Swapchain:
                    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    attachmentDescription.format      = surfaceFormat;
                    break;
            }

            attachmentDescriptions.emplace_back(attachmentDescription);
        }

        // Creates each subpass and its dependencies.
        std::vector<std::unique_ptr<SubpassDescription>> subpasses;
        std::vector<VkSubpassDependency> dependencies;

        for (const auto &subpassType: renderStage.GetSubpasses())
        {
            // Attachments.
            std::vector<VkAttachmentReference> subpassColourAttachments;

            std::optional<uint32_t> depthAttachment;

            for (const auto &attachmentBinding: subpassType.GetAttachmentBindings())
            {
                auto attachment = renderStage.GetAttachment(attachmentBinding);

                if (!attachment)
                {
                    Log::Error("Failed to find a renderpass attachment bound to: ", attachmentBinding, '\n');
                    continue;
                }

                if (attachment->GetType() == RhiAttachment::Type::Depth)
                {
                    depthAttachment = attachment->GetBinding();
                    continue;
                }

                VkAttachmentReference attachmentReference = {};
                attachmentReference.attachment            = attachment->GetBinding();
                attachmentReference.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                subpassColourAttachments.emplace_back(attachmentReference);
            }

            // Subpass description.
            subpasses.emplace_back(std::make_unique<SubpassDescription>(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                                        subpassColourAttachments, depthAttachment));

            // Subpass dependencies.
            //
            // One VkSubpassDependency per boundary this subpass touches, not
            // per subpass. A subpass that is BOTH the first and the last
            // (i.e. a render stage with exactly one subpass) touches TWO
            // boundaries; external->this and this->external; and needs a
            // separate VkSubpassDependency for each. Cramming both into a
            // single struct (the old code) works only when first != last;
            // when they coincide, the second block's writes unconditionally
            // overwrite the first's, producing one dependency with BOTH
            // srcSubpass and dstSubpass set to VK_SUBPASS_EXTERNAL; which
            // doesn't order anything against the subpass's actual work on
            // either side. That silently drops the "finish writing before
            // present" guarantee for any single-subpass stage (e.g. a
            // tonemap-only stage writing "swapchain"), which can look correct
            // on the first frame and then race/corrupt once the GPU starts
            // genuinely overlapping frame execution.
            const uint32_t binding     = subpassType.GetBinding();
            const uint32_t lastBinding = static_cast<uint32_t>(renderStage.GetSubpasses().size()) - 1;

            // Dependency INTO this subpass, from whatever precedes it
            // (external if this is the first subpass, otherwise the
            // previous subpass).
            {
                VkSubpassDependency dep = {};
                dep.dstSubpass          = binding;
                dep.dstStageMask =
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

                if (binding == 0)
                {
                    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
                    dep.srcStageMask =
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                } else
                {
                    dep.srcSubpass = binding - 1;
                    dep.srcStageMask =
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    dep.srcAccessMask =
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }

                dependencies.emplace_back(dep);
            }

            // Dependency OUT of this subpass to external, ONLY if this is
            // also the last subpass; a separate entry from the one above,
            // not folded into it, so the single-subpass case gets both.
            if (binding == lastBinding)
            {
                VkSubpassDependency dep = {};
                dep.srcSubpass          = binding;
                dep.dstSubpass          = VK_SUBPASS_EXTERNAL;
                dep.srcStageMask =
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dep.dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dep.dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
                dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

                dependencies.emplace_back(dep);
            }
        }

        std::vector<VkSubpassDescription> subpassDescriptions;
        subpassDescriptions.reserve(subpasses.size());

        for (const auto &subpass: subpasses)
        {
            subpassDescriptions.emplace_back(subpass->GetSubpassDescription());
        }

        // Creates the render pass.
        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount        = static_cast<uint32_t>(attachmentDescriptions.size());
        renderPassCreateInfo.pAttachments           = attachmentDescriptions.data();
        renderPassCreateInfo.subpassCount           = static_cast<uint32_t>(subpassDescriptions.size());
        renderPassCreateInfo.pSubpasses             = subpassDescriptions.data();
        renderPassCreateInfo.dependencyCount        = static_cast<uint32_t>(dependencies.size());
        renderPassCreateInfo.pDependencies          = dependencies.data();
        RenderSystem::CheckVkResult(vkCreateRenderPass(logicalDevice, &renderPassCreateInfo, nullptr, &renderpass));
    }

    RhiRenderpass::~RhiRenderpass() { vkDestroyRenderPass(logicalDevice, renderpass, nullptr); }
}