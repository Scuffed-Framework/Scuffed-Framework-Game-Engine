#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <vector>

namespace SF::Engine
{
    class Instance;
    class PhysicalDevice;

    class LogicalDevice
    {
        friend class RenderSystem;

    public:
        LogicalDevice(const Instance& instance, const PhysicalDevice& physicalDevice);
        ~LogicalDevice();

        operator const VkDevice&() const
        {
            return logicalDevice;
        }

        const VkDevice& GetLogicalDevice() const
        {
            return logicalDevice;
        }
        const VkPhysicalDeviceFeatures& GetEnabledFeatures() const
        {
            return enabledFeatures;
        }
        const VkQueue& GetGraphicsQueue() const
        {
            return GraphicsQueue;
        }
        const VkQueue& GetPresentQueue() const
        {
            return presentQueue;
        }
        const VkQueue& GetComputeQueue() const
        {
            return computeQueue;
        }
        const VkQueue& GetTransferQueue() const
        {
            return transferQueue;
        }
        uint32_t GetGraphicsFamily() const
        {
            return GraphicsFamily;
        }
        uint32_t GetPresentFamily() const
        {
            return presentFamily;
        }
        uint32_t GetComputeFamily() const
        {
            return computeFamily;
        }
        uint32_t GetTransferFamily() const
        {
            return transferFamily;
        }

        static const std::vector<const char*> DeviceExtensions;

    private:
        void CreateQueueIndices();
        void CreateLogicalDevice();

        const Instance& instance;
        const PhysicalDevice& physicalDevice;

        VkDevice logicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures enabledFeatures = {};

        VkQueueFlags supportedQueues = {};
        uint32_t GraphicsFamily = 0;
        uint32_t presentFamily = 0;
        uint32_t computeFamily = 0;
        uint32_t transferFamily = 0;

        VkQueue GraphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        VkQueue computeQueue = VK_NULL_HANDLE;
        VkQueue transferQueue = VK_NULL_HANDLE;
    };
}