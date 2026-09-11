#pragma once

#define VK_NO_PROTOTYPES

#include <vector>
#include <volk.h>

namespace SF::Engine
{
    class Instance;
    class PhysicalDevice;

    class LogicalDevice
    {
        friend class RenderSystem;

    public:
        LogicalDevice(const Instance &instance, const PhysicalDevice &physicalDevice);
        ~LogicalDevice();

        operator const VkDevice &() const { return logicalDevice; }

        [[nodiscard]] const VkDevice &GetLogicalDevice() const { return logicalDevice; }
        [[nodiscard]] const VkPhysicalDeviceFeatures &GetEnabledFeatures() const { return enabledFeatures; }
        [[nodiscard]] const VkQueue &GetGraphicsQueue() const { return GraphicsQueue; }
        [[nodiscard]] const VkQueue &GetPresentQueue() const { return presentQueue; }
        [[nodiscard]] const VkQueue &GetComputeQueue() const { return computeQueue; }
        [[nodiscard]] const VkQueue &GetTransferQueue() const { return transferQueue; }
        [[nodiscard]] uint32_t GetGraphicsFamily() const { return GraphicsFamily; }
        [[nodiscard]] uint32_t GetPresentFamily() const { return presentFamily; }
        [[nodiscard]] uint32_t GetComputeFamily() const { return computeFamily; }
        [[nodiscard]] uint32_t GetTransferFamily() const { return transferFamily; }

        static const std::vector<const char *> DeviceExtensions;

    private:
        void CreateQueueIndices();
        void CreateLogicalDevice();

        const Instance &instance;
        const PhysicalDevice &physicalDevice;

        VkDevice logicalDevice                   = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures enabledFeatures = {};

        VkQueueFlags supportedQueues = {};
        uint32_t GraphicsFamily      = 0;
        uint32_t presentFamily       = 0;
        uint32_t computeFamily       = 0;
        uint32_t transferFamily      = 0;

        VkQueue GraphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue  = VK_NULL_HANDLE;
        VkQueue computeQueue  = VK_NULL_HANDLE;
        VkQueue transferQueue = VK_NULL_HANDLE;
    };
} // namespace SF::Engine
