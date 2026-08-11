#include "CommandBuffer.hpp"

#include <Rendering/RenderSystem.hpp>

namespace SF::Engine
{
    CommandBuffer::CommandBuffer(bool begin, VkQueueFlagBits queueType,
                                 VkCommandBufferLevel bufferLevel)
        : commandPool(RenderSystem::Get()->GetCommandPool()), queueType(queueType)
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = *commandPool;
        commandBufferAllocateInfo.level = bufferLevel;
        commandBufferAllocateInfo.commandBufferCount = 1;
        RenderSystem::CheckVkResult(
            vkAllocateCommandBuffers(*logicalDevice, &commandBufferAllocateInfo, &commandBuffer));

        if (begin) Begin();
    }

    CommandBuffer::~CommandBuffer()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        vkFreeCommandBuffers(*logicalDevice, commandPool->GetCommandPool(), 1, &commandBuffer);
    }

    void CommandBuffer::Begin(VkCommandBufferUsageFlags usage)
    {
        if (running) return;

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = usage;
        RenderSystem::CheckVkResult(vkBeginCommandBuffer(commandBuffer, &beginInfo));
        running = true;
    }

    void CommandBuffer::End()
    {
        if (!running) return;

        RenderSystem::CheckVkResult(vkEndCommandBuffer(commandBuffer));
        running = false;
    }

    void CommandBuffer::SubmitIdle()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto queueSelected = GetQueue();

        if (running) End();

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkFence fence;
        RenderSystem::CheckVkResult(
            vkCreateFence(*logicalDevice, &fenceCreateInfo, nullptr, &fence));

        RenderSystem::CheckVkResult(vkResetFences(*logicalDevice, 1, &fence));

        RenderSystem::CheckVkResult(vkQueueSubmit(queueSelected, 1, &submitInfo, fence));

        RenderSystem::CheckVkResult(vkWaitForFences(*logicalDevice, 1, &fence, VK_TRUE,
                                                    std::numeric_limits<uint64_t>::max()));

        vkDestroyFence(*logicalDevice, fence, nullptr);
    }

    void CommandBuffer::Submit(const VkSemaphore& waitSemaphore, const VkSemaphore& signalSemaphore,
                               VkFence fence)
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto queueSelected = GetQueue();

        if (running) End();

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (waitSemaphore != VK_NULL_HANDLE)
        {
            // Pipeline stages used to wait at for graphics queue submissions.
            static VkPipelineStageFlags submitPipelineStages =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            submitInfo.pWaitDstStageMask = &submitPipelineStages;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &waitSemaphore;
        }

        if (signalSemaphore != VK_NULL_HANDLE)
        {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSemaphore;
        }

        if (fence != VK_NULL_HANDLE)
            RenderSystem::CheckVkResult(vkResetFences(*logicalDevice, 1, &fence));

        RenderSystem::CheckVkResult(vkQueueSubmit(queueSelected, 1, &submitInfo, fence));
    }

    VkQueue CommandBuffer::GetQueue() const
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        switch (queueType)
        {
            case VK_QUEUE_GRAPHICS_BIT:
                return logicalDevice->GetGraphicsQueue();
            case VK_QUEUE_COMPUTE_BIT:
                // The engine has a single command pool created for the graphics
                // queue family. Submitting to the dedicated compute queue (a
                // different family) causes VUID-vkQueueSubmit-pCommandBuffers-00074.
                // The graphics queue supports compute operations, so submit there.
                return logicalDevice->GetGraphicsQueue();
            default:
                return nullptr;
        }
    }
}