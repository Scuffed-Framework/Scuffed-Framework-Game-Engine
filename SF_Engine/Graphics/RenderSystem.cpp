#include <string>
#include <vector>

#include "RenderSystem.hpp"

#include <cstring>

#include <glslang/Public/ShaderLang.h>
#include "PipelineRenderer.hpp"
#include "Windows/Windows.hpp"

#include <glslang/Include/visibility.h>
namespace SF::Engine
{
    RenderSystem::RenderSystem()
        : elapsedPurge(5s),
          instance(std::make_unique<Instance>()),
          physicalDevice(std::make_unique<PhysicalDevice>(*instance)),
          logicalDevice(std::make_unique<LogicalDevice>(*instance, *physicalDevice))
    {
        WindowManager::Get()->OnAddWindow().connect(
            [this](Window *window, bool added)
            {
                surfaces.emplace_back(
                    std::make_unique<Surface>(*instance, *physicalDevice, *logicalDevice, *window));
            });

        // Initialize VMA allocator - required by all Image creation/destruction
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorCreateInfo.physicalDevice = *physicalDevice;
        allocatorCreateInfo.device = *logicalDevice;
        allocatorCreateInfo.instance = *instance;

        // Wire up volk-loaded function pointers into VMA
        VmaVulkanFunctions vmaVulkanFunctions = {};
        vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        allocatorCreateInfo.pVulkanFunctions = &vmaVulkanFunctions;

        Log::Info("Creating VMA allocator");
        VkResult vmaResult = vmaCreateAllocator(&allocatorCreateInfo, &alloc);
        if (vmaResult != VK_SUCCESS)
            throw std::runtime_error("Failed to create VMA allocator: " + StrVkResult(vmaResult));
        Log::Info("VMA allocator created");

        CreatePipelineCache();
        Log::Info("Pipeline cache created");

        if (!glslang::InitializeProcess())
            throw std::runtime_error("Failed to initialize glslang process");
        Log::Info("RenderSystem fully initialized");
    }

    RenderSystem::~RenderSystem()
    {
        // Wait for ALL GPU work before destroying anything
        vkDeviceWaitIdle(*logicalDevice);

        renderer = nullptr;
        swapchains.clear();

        glslang::FinalizeProcess();

        vkDestroyPipelineCache(*logicalDevice, pipelineCache, nullptr);

        vmaDestroyAllocator(alloc);

        commandPools.clear();

        for (auto &perSurfaceBuffer : perSurfaceBuffers)
        {
            for (std::size_t i = 0; i < perSurfaceBuffer->flightFences.size(); i++)
            {
                vkDestroyFence(*logicalDevice, perSurfaceBuffer->flightFences[i], nullptr);
                vkDestroySemaphore(*logicalDevice, perSurfaceBuffer->renderCompletes[i], nullptr);
                vkDestroySemaphore(*logicalDevice, perSurfaceBuffer->presentCompletes[i], nullptr);
            }

            perSurfaceBuffer->commandBuffers.clear();
        }
    }

    void RenderSystem::Update()
    {
        if (!renderer || WindowManager::Get()->GetWindow(0)->IsIconified())
            return;

        if (!renderer->started)
        {
            ResetRenderStages();
            renderer->Start();
            renderer->started = true;
        }

        renderer->Update();

        for (auto [id, swapchain] : Enumerate(swapchains))
        {
            auto &perSurfaceBuffer = perSurfaceBuffers[id];
            auto acquireResult = swapchain->AcquireNextImage(
                perSurfaceBuffer->presentCompletes[perSurfaceBuffer->currentFrame],
                perSurfaceBuffer->flightFences[perSurfaceBuffer->currentFrame]);

            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                RecreateSwapchain();
                return;
            }
            // Suboptimal: keep going with current swapchain : RecreatePass will
            // handle the rebuild once the size stabilises (debounce)
            if (acquireResult == VK_SUBOPTIMAL_KHR)
            {
                perSurfaceBuffer->framebufferResized = true;
            }

            if (acquireResult != VK_SUCCESS)
            {
                Log::Error("Failed to acquire swap chain image!\n");
                return;
            }

            Pipeline::Stage stage;

            for (auto &renderStage : renderer->renderStages)
            {
                renderStage->Update();

                // Open the command buffer early so PreRender() can record
                // compute work (cluster culling, transitions, etc.) before
                // the renderpass begins.
                auto &commandBuffer =
                    perSurfaceBuffer->commandBuffers[swapchain->GetActiveImageIndex()];

                if (!commandBuffer->IsRunning())
                    commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

                // Call PreRender on all PipelinePasss for every subpass in this
                // stage : runs OUTSIDE the renderpass (safe for compute).
                for (const auto &subpass : renderStage->GetSubpasses())
                {
                    stage.second = subpass.GetBinding();
                    renderer->PipelinePassManager.PreRenderStage(stage, *commandBuffer);
                }
                stage.second = 0;

                if (!StartRenderpass(id, *renderStage))
                    return;

                for (const auto &subpass : renderStage->GetSubpasses())
                {
                    stage.second = subpass.GetBinding();

                    // Renders subpass PipelinePass pipelines.
                    renderer->PipelinePassManager.RenderStage(stage, *commandBuffer);

                    if (subpass.GetBinding() != renderStage->GetSubpasses().back().GetBinding())
                        vkCmdNextSubpass(*commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
                }

                EndRenderpass(id, *renderStage);
                stage.first++;
            }
        }

        // Purges unused command pools.
        if (elapsedPurge.GetElapsed() != 0)
        {
            for (auto it = commandPools.begin(); it != commandPools.end();)
            {
                if ((*it).second.use_count() <= 1)
                {
                    it = commandPools.erase(it);
                    continue;
                }

                ++it;
            }
        }
    }

    std::string RenderSystem::StrVkResult(VkResult result)
    {
        switch (result)
        {
        case VK_SUCCESS:
            return "Success";
        case VK_NOT_READY:
            return "A fence or query has not yet completed";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time";
        case VK_EVENT_SET:
            return "An event is signaled";
        case VK_EVENT_RESET:
            return "An event is unsignaled";
        case VK_INCOMPLETE:
            return "A return array was too small for the result";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for "
                   "implementation-specific reasons";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is "
                   "otherwise incompatible";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can "
                   "still be used";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with "
                   "the swapchain";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image "
                   "layout";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some "
                   "other non-Vulkan API";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "A validation layer found an error";
        default:
            return "Unknown Vulkan error";
        }
    }

    void RenderSystem::CheckVkResult(VkResult result)
    {
        if (result >= 0)
            return;

        auto failure = StrVkResult(result);

        throw std::runtime_error("Vulkan error: " + failure);
    }

    void RenderSystem::CaptureScreenshot(const std::filesystem::path &filename,
                                         std::size_t id) const
    {
        auto size = WindowManager::Get()->GetWindow(0)->GetSize();

        VkImage dstImage;
        VmaAllocation dstImageMemory;

        auto supportsBlit =
            Image::CopyImage(swapchains[id]->GetActiveImage(), dstImage, dstImageMemory,
                             surfaces[id]->GetFormat().format, {size.x, size.y, 1},
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0);

        VkImageSubresource imageSubresource{};
        imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkSubresourceLayout dstSubresourceLayout{};
        vkGetImageSubresourceLayout(*logicalDevice, dstImage, &imageSubresource,
                                    &dstSubresourceLayout);

        Bitmap bitmap(std::make_unique<uint8_t[]>(dstSubresourceLayout.size), size);

        void *data = nullptr;
        vmaMapMemory(alloc, dstImageMemory, &data);

        std::memcpy(bitmap.GetData().get(),
                    static_cast<uint8_t *>(data) + dstSubresourceLayout.offset,
                    static_cast<size_t>(dstSubresourceLayout.size));

        vmaUnmapMemory(alloc, dstImageMemory);
        vmaDestroyImage(alloc, dstImage, dstImageMemory);
        bitmap.Write(filename);
    }

    const RenderStage *RenderSystem::GetRenderStage(uint32_t index) const
    {
        if (renderer)
            return renderer->GetRenderStage(index);
        return nullptr;
    }

    const Descriptor *RenderSystem::GetAttachment(const std::string &name) const
    {
        if (auto it = attachments.find(name); it != attachments.end())
            return it->second;
        return nullptr;
    }

    const std::shared_ptr<CommandPool> &RenderSystem::GetCommandPool(
        const std::thread::id &threadId)
    {
        if (auto it = commandPools.find(threadId); it != commandPools.end())
            return it->second;
        return commandPools.emplace(threadId, std::make_shared<CommandPool>(threadId))
            .first->second;
    }

    void RenderSystem::CreatePipelineCache()
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        CheckVkResult(vkCreatePipelineCache(*logicalDevice, &pipelineCacheCreateInfo, nullptr,
                                            &pipelineCache));
    }

    void RenderSystem::ResetRenderStages()
    {
        // Update render stages first so extents are populated from the window size
        for (const auto &renderStage : renderer->renderStages)
            renderStage->Update();

        RecreateSwapchain();

        for (const auto [id, swapchain] : Enumerate(swapchains))
        {
            auto &perSurfaceBuffer = perSurfaceBuffers[id];
            if (perSurfaceBuffer->flightFences.size() != swapchain->GetImageCount())
                RecreateCommandBuffers(id);

            for (const auto &renderStage : renderer->renderStages)
                renderStage->Rebuild(*swapchain);
        }

        RecreateAttachmentsMap();
    }

    void RenderSystem::RecreateSwapchain()
    {
        // Wait until the window has a valid size (handles minimize/restore)
        auto window = WindowManager::Get()->GetWindow(0);
        while (window->GetSize().x == 0 || window->GetSize().y == 0)
            glfwWaitEvents();

        vkDeviceWaitIdle(*logicalDevice);

        VkExtent2D displayExtent = {WindowManager::Get()->GetWindow(0)->GetSize().x,
                                    WindowManager::Get()->GetWindow(0)->GetSize().y};
        swapchains.resize(surfaces.size());
        perSurfaceBuffers.resize(surfaces.size());
        for (const auto [id, surface] : Enumerate(surfaces))
        {
            swapchains[id] = std::make_unique<Swapchain>(*physicalDevice, *surface, *logicalDevice,
                                                         displayExtent, swapchains[id].get());

            // Explicitly destroy semaphores and fences before replacing the buffer set.
            // Simply assigning a new PerSurfaceBuffers leaks them because the Vulkan
            // handles are plain VkSemaphore/VkFence values with no RAII wrappers.
            if (perSurfaceBuffers[id])
            {
                auto &psb = *perSurfaceBuffers[id];
                for (std::size_t i = 0; i < psb.flightFences.size(); i++)
                {
                    vkDestroyFence(*logicalDevice, psb.flightFences[i], nullptr);
                    vkDestroySemaphore(*logicalDevice, psb.renderCompletes[i], nullptr);
                    vkDestroySemaphore(*logicalDevice, psb.presentCompletes[i], nullptr);
                }
                psb.flightFences.clear();
                psb.renderCompletes.clear();
                psb.presentCompletes.clear();
                psb.commandBuffers.clear();
            }

            perSurfaceBuffers[id] = std::make_unique<PerSurfaceBuffers>();
            RecreateCommandBuffers(id);
        }

        vkDeviceWaitIdle(*logicalDevice);
    }

    void RenderSystem::RecreateCommandBuffers(std::size_t id)
    {
        auto &swapchain = swapchains[id];
        auto &perSurfaceBuffer = perSurfaceBuffers[id];

        for (std::size_t i = 0; i < perSurfaceBuffer->flightFences.size(); i++)
        {
            vkDestroyFence(*logicalDevice, perSurfaceBuffer->flightFences[i], nullptr);
            vkDestroySemaphore(*logicalDevice, perSurfaceBuffer->renderCompletes[i], nullptr);
            vkDestroySemaphore(*logicalDevice, perSurfaceBuffer->presentCompletes[i], nullptr);
        }

        perSurfaceBuffer->presentCompletes.resize(swapchain->GetImageCount());
        perSurfaceBuffer->renderCompletes.resize(swapchain->GetImageCount());
        perSurfaceBuffer->flightFences.resize(swapchain->GetImageCount());
        perSurfaceBuffer->commandBuffers.resize(swapchain->GetImageCount());

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t i = 0; i < perSurfaceBuffer->flightFences.size(); i++)
        {
            CheckVkResult(vkCreateSemaphore(*logicalDevice, &semaphoreCreateInfo, nullptr,
                                            &perSurfaceBuffer->presentCompletes[i]));
            CheckVkResult(vkCreateSemaphore(*logicalDevice, &semaphoreCreateInfo, nullptr,
                                            &perSurfaceBuffer->renderCompletes[i]));
            CheckVkResult(vkCreateFence(*logicalDevice, &fenceCreateInfo, nullptr,
                                        &perSurfaceBuffer->flightFences[i]));
            perSurfaceBuffer->commandBuffers[i] = std::make_unique<CommandBuffer>(false);
        }
    }

    void RenderSystem::RecreatePass(std::size_t id, RenderStage &renderStage)
    {
        VkExtent2D displayExtent = {WindowManager::Get()->GetWindow(0)->GetSize().x,
                                    WindowManager::Get()->GetWindow(0)->GetSize().y};

        CheckVkResult(vkQueueWaitIdle(logicalDevice->GetGraphicsQueue()));

        bool needsSwapchainRecreate = false;
        for (const auto [sid, swapchain] : Enumerate(swapchains))
        {
            auto &psb = perSurfaceBuffers[sid];
            if (renderStage.HasSwapchain() &&
                (psb->framebufferResized || !swapchain->IsSameExtent(displayExtent)))
            {
                psb->framebufferResized = false;
                needsSwapchainRecreate = true;
            }
        }

        if (needsSwapchainRecreate)
            RecreateSwapchain();

        // Rebuild ALL render stages and command buffers
        for (const auto [sid, swapchain] : Enumerate(swapchains))
        {
            if (perSurfaceBuffers[sid]->flightFences.size() != swapchain->GetImageCount())
                RecreateCommandBuffers(sid);

            for (const auto &stage : renderer->renderStages)
                stage->Rebuild(*swapchain);
        }

        RecreateAttachmentsMap();
    }

    void RenderSystem::RecreateAttachmentsMap()
    {
        attachments.clear();

        for (const auto &renderStage : renderer->renderStages)
            attachments.insert(renderStage->descriptors.begin(), renderStage->descriptors.end());
    }

    bool RenderSystem::StartRenderpass(std::size_t id, RenderStage &renderStage)
    {
        if (renderStage.IsOutOfDate())
        {
            RecreatePass(id, renderStage);
            return false;
        }

        auto &swapchain = swapchains[id];
        auto &perSurfaceBuffer = perSurfaceBuffers[id];
        auto &commandBuffer = perSurfaceBuffer->commandBuffers[swapchain->GetActiveImageIndex()];

        if (!commandBuffer->IsRunning())
            commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

        // Always use swapchain extent for the renderpass : the render stage render area
        // may have updated to the new window size before the swapchain was rebuilt
        auto scExtent = swapchain->GetExtent();
        VkRect2D renderArea = {};
        renderArea.offset = {0, 0};
        renderArea.extent = scExtent;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(scExtent.width);
        viewport.height = static_cast<float>(scExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(*commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = renderArea.offset;
        scissor.extent = renderArea.extent;
        vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);

        auto clearValues = renderStage.GetClearValues();

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = *renderStage.GetRenderpass();
        renderPassBeginInfo.framebuffer =
            renderStage.GetActiveFramebuffer(swapchain->GetActiveImageIndex());
        renderPassBeginInfo.renderArea = renderArea;
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassBeginInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(*commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        return true;
    }

    void RenderSystem::EndRenderpass(std::size_t id, RenderStage &renderStage)
    {
        auto presentQueue = logicalDevice->GetPresentQueue();
        auto &swapchain = swapchains[id];
        auto &perSurfaceBuffer = perSurfaceBuffers[id];
        auto &commandBuffer = perSurfaceBuffer->commandBuffers[swapchain->GetActiveImageIndex()];

        vkCmdEndRenderPass(*commandBuffer);

        if (!renderStage.HasSwapchain())
            return;

        commandBuffer->End();
        commandBuffer->Submit(perSurfaceBuffer->presentCompletes[perSurfaceBuffer->currentFrame],
                              perSurfaceBuffer->renderCompletes[perSurfaceBuffer->currentFrame],
                              perSurfaceBuffer->flightFences[perSurfaceBuffer->currentFrame]);

        auto presentResult = swapchain->QueuePresent(
            presentQueue, perSurfaceBuffer->renderCompletes[perSurfaceBuffer->currentFrame]);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            perSurfaceBuffer->framebufferResized = true;
        }
        else if (presentResult != VK_SUCCESS)
        {
            CheckVkResult(presentResult);
            Log::Error("Failed to present swap chain image!\n");
        }

        perSurfaceBuffer->currentFrame =
            (perSurfaceBuffer->currentFrame + 1) % swapchain->GetImageCount();
    }
}
