#include "CommandPool.hpp"

#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    CommandPool::CommandPool(const std::thread::id& threadId) : threadId(threadId)
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto GraphicsFamily = logicalDevice->GetGraphicsFamily();

        VkCommandPoolCreateInfo commandPoolCreateInfo = {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolCreateInfo.queueFamilyIndex = GraphicsFamily;
        RenderSystem::CheckVkResult(
            vkCreateCommandPool(*logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool));
    }

    CommandPool::~CommandPool()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        vkDestroyCommandPool(*logicalDevice, commandPool, nullptr);
    }
}