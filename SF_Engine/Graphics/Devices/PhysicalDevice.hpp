#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <vector>

namespace SF::Engine
{
    class Instance;

    class PhysicalDevice
    {
        friend class RenderSystem;

    public:
        explicit PhysicalDevice(const Instance &instance);

        operator const VkPhysicalDevice &() const
        {
            return physicalDevice;
        }

        const VkPhysicalDevice &GetPhysicalDevice() const
        {
            return physicalDevice;
        }
        const VkPhysicalDeviceProperties &GetProperties() const
        {
            return properties;
        }
        const VkPhysicalDeviceFeatures &GetFeatures() const
        {
            return features;
        }
        const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const
        {
            return memoryProperties;
        }
        const VkSampleCountFlagBits &GetMsaaSamples() const
        {
            return msaaSamples;
        }

        // Vulkan 1.1+ feature getters
        const VkPhysicalDeviceVulkan11Features &GetVulkan11Features() const
        {
            return vulkan11Features;
        }
        const VkPhysicalDeviceVulkan12Features &GetVulkan12Features() const
        {
            return vulkan12Features;
        }
        const VkPhysicalDeviceVulkan13Features &GetVulkan13Features() const
        {
            return vulkan13Features;
        }

        const VkPhysicalDeviceVulkan11Properties &GetVulkan11Properties() const
        {
            return vulkan11Properties;
        }
        const VkPhysicalDeviceVulkan12Properties &GetVulkan12Properties() const
        {
            return vulkan12Properties;
        }
        const VkPhysicalDeviceVulkan13Properties &GetVulkan13Properties() const
        {
            return vulkan13Properties;
        }

        const VkPhysicalDeviceDescriptorIndexingProperties GetDescriptorIndexingProperties() const { return indexingProperties; }
        const VkPhysicalDeviceProperties2 GetDeviceProperties2() const { return properties2; }

    private:
        void QueryDeviceProperties();
        void QueryDeviceFeatures();
        VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice> &devices);
        static uint32_t EnumeratePhysicalDevice(const VkPhysicalDevice &device);
        VkSampleCountFlagBits GetMaxUsableSampleCount() const;
        void LogDeviceInfo() const;

        static void LogVulkanDevice(const VkPhysicalDeviceProperties &deviceProperties,
                                    const std::vector<VkExtensionProperties> &extensionProperties);

        const Instance &instance;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        // Core properties and features
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};

        // Vulkan 1.1 features and properties
        VkPhysicalDeviceVulkan11Features vulkan11Features = {};
        VkPhysicalDeviceVulkan11Properties vulkan11Properties = {};

        // Vulkan 1.2 features and properties
        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        VkPhysicalDeviceVulkan12Properties vulkan12Properties = {};

        // Vulkan 1.3 features and properties
        VkPhysicalDeviceVulkan13Features vulkan13Features = {};
        VkPhysicalDeviceVulkan13Properties vulkan13Properties = {};

        VkPhysicalDeviceDescriptorIndexingProperties indexingProperties = {};
        VkPhysicalDeviceProperties2 properties2 = {};

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    };
}