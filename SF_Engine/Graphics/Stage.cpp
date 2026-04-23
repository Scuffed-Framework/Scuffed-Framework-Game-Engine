#include "Stage.hpp"
#include "Images/ImageDepth.hpp"
#include "RenderSystem.hpp"
#include "Windows/Windows.hpp"

namespace SF::Engine
{
    RenderStage::RenderStage(std::vector<Attachment> images, std::vector<SubpassType> subpasses,
                             const Viewport &viewport)
        : attachments(std::move(images)), subpasses(std::move(subpasses)), viewport(viewport)
    {
        // Scan attachments to find the depth and swapchain attachments
        for (const auto &attachment : attachments)
        {
            if (attachment.GetType() == Attachment::Type::Depth)
                depthAttachment = attachment;
            else if (attachment.GetType() == Attachment::Type::Swapchain)
                swapchainAttachment = attachment;
        }

        // Build clear values and per-subpass metadata
        for (const auto &subpass : this->subpasses)
        {
            uint32_t count = 0;
            bool multisampled = false;

            for (auto bindingIndex : subpass.GetAttachmentBindings())
            {
                auto att = GetAttachment(bindingIndex);
                if (!att)
                    continue;

                VkClearValue clearValue = {};
                if (att->GetType() == Attachment::Type::Depth)
                {
                    clearValue.depthStencil = {1.0f, 0};
                }
                else
                {
                    auto c = att->GetClearColor();
                    clearValue.color = {c.r, c.g, c.b, c.a};
                }
                clearValues.emplace_back(clearValue);
                count++;

                if (att->IsMultisampled())
                    multisampled = true;
            }

            subpassAttachmentCount.emplace_back(count);
            subpassMultisampled.emplace_back(multisampled);
        }
    }

    void RenderStage::Update()
    {
        auto lastRenderArea = renderArea;

        renderArea.SetOffset(viewport.GetOffset());

        if (viewport.GetSize())
            renderArea.SetExtent(
                Vector2Uint(viewport.GetScale() * Vector2float(*viewport.GetSize())));
        else
            renderArea.SetExtent(Vector2Uint(
                viewport.GetScale() * Vector2float(WindowManager::Get()->GetWindow(0)->GetSize())));

        // Don't mark as out of date with a zero extent : window is probably minimized.
        // Rebuilding with zero extent creates an invalid framebuffer and crashes.
        if (renderArea.GetExtent().x == 0 || renderArea.GetExtent().y == 0)
        {
            outOfDate = false;
            return;
        }

        renderArea.SetAspectRatio(static_cast<float>(renderArea.GetExtent().x) /
                                  static_cast<float>(renderArea.GetExtent().y));
        renderArea.SetExtent(renderArea.GetExtent() + renderArea.GetOffset());

        outOfDate = renderArea != lastRenderArea;
    }

    void RenderStage::Rebuild(const Swapchain &swapchain)
    {
        auto physicalDevice = RenderSystem::Get()->GetPhysicalDevice();
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto surface = RenderSystem::Get()->GetSurface(0);

        auto msaaSamples = physicalDevice->GetMsaaSamples();
        Log::Info("RenderStage::Rebuild extent={}x{}", renderArea.GetExtent().x, renderArea.GetExtent().y);

        if (depthAttachment)
        {
            Log::Info("Creating ImageDepth");
            depthStencil = std::make_unique<ImageDepth>(
                renderArea.GetExtent(),
                depthAttachment->IsMultisampled() ? msaaSamples : VK_SAMPLE_COUNT_1_BIT);
            Log::Info("ImageDepth created");
        }

        // Always recreate the renderpass so it uses the correct depth format.
        // The old guard `if (!renderpass)` caused it to reuse a stale renderpass
        // built before depthStencil existed, producing VK_FORMAT_R4G4_UNORM_PACK8.
        Log::Info("Creating Renderpass");
        renderpass = std::make_unique<Renderpass>(
            *logicalDevice, *this,
            depthStencil ? depthStencil->GetFormat() : VK_FORMAT_UNDEFINED,
            surface->GetFormat().format, msaaSamples);
        Log::Info("Renderpass created");

        Log::Info("Creating Framebuffer");
        framebuffer =
            std::make_unique<Framebuffer>(*logicalDevice, swapchain, *this, *renderpass,
                                          *depthStencil, renderArea.GetExtent(), msaaSamples);
        Log::Info("Framebuffer created");
        outOfDate = false;

        descriptors.clear();
        auto where = descriptors.end();

        for (const auto &image : attachments)
        {
            if (image.GetType() == Attachment::Type::Depth)
                where = descriptors.insert(where, {image.GetName(), depthStencil.get()});
            else
                where = descriptors.insert(
                    where, {image.GetName(), framebuffer->GetAttachment(image.GetBinding())});
        }
    }

    std::optional<Attachment> RenderStage::GetAttachment(const std::string &name) const
    {
        for (const auto &attachment : attachments)
        {
            if (attachment.GetName() == name)
            {
                return attachment;
            }
        }
        return std::nullopt;
    }

    std::optional<Attachment> RenderStage::GetAttachment(uint32_t binding) const
    {
        for (const auto &attachment : attachments)
        {
            if (attachment.GetBinding() == binding)
            {
                return attachment;
            }
        }
        return std::nullopt;
    }

    const Descriptor *RenderStage::GetDescriptor(const std::string &name) const
    {
        auto it = descriptors.find(name);
        if (it != descriptors.end())
        {
            return it->second;
        }
        return nullptr;
    }

    const VkFramebuffer &RenderStage::GetActiveFramebuffer(uint32_t activeSwapchainImage) const
    {
        return framebuffer->GetFramebuffer()[activeSwapchainImage];
    }
}