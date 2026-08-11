#include "PhysicalDevice.hpp"

#include <fmt/format.h>
#include <algorithm>

#include "Graphics/RenderSystem.hpp"
#include "Instance.hpp"
#include "LogicalDevice.hpp"

namespace SF::Engine
{
    static const std::vector<VkSampleCountFlagBits> STAGE_FLAG_BITS = {
        VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT};

    PhysicalDevice::PhysicalDevice(const Instance &instance) : instance(instance)
    {
        uint32_t physicalDeviceCount;
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

        if (physicalDeviceCount == 0)
            throw std::runtime_error("Failed to find GPUs with Vulkan support");

        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

        physicalDevice = ChoosePhysicalDevice(physicalDevices);
        if (!physicalDevice)
            throw std::runtime_error("Failed to find a suitable GPU");

        // Query all properties using Vulkan 1.1+ feature chain
        QueryDeviceProperties();
        QueryDeviceFeatures();

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        msaaSamples = GetMaxUsableSampleCount();

        Log::Out(fmt::runtime("Selected Physical Device: {} \"{}\""), properties.deviceID,
                 properties.deviceName);

        LogDeviceInfo();
    }

    void PhysicalDevice::QueryDeviceProperties()
    {
        // Query Vulkan 1.3 properties
        vulkan13Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;

        // Query Vulkan 1.2 properties
        vulkan12Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
        vulkan12Properties.pNext = &vulkan13Properties;

        // Query Vulkan 1.1 properties
        vulkan11Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        vulkan11Properties.pNext = &vulkan12Properties;

        // Query base properties with chain
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &vulkan11Properties;

        vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);
        properties = properties2.properties;

        indexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
        indexingProperties.pNext = 0;
    }

    void PhysicalDevice::QueryDeviceFeatures()
    {
        // Query Vulkan 1.3 features
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        // Query Vulkan 1.2 features
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.pNext = &vulkan13Features;

        // Query Vulkan 1.1 features
        vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan11Features.pNext = &vulkan12Features;

        // Query base features with chain
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &vulkan11Features;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
        features = features2.features;
    }

    VkPhysicalDevice PhysicalDevice::ChoosePhysicalDevice(
        const std::vector<VkPhysicalDevice> &devices)
    {
        // Store devices with their scores
        std::multimap<uint32_t, VkPhysicalDevice> rankedDevices;

        for (const auto &device : devices)
        {
            uint32_t score = EnumeratePhysicalDevice(device);
            if (score > 0) // Only consider suitable devices
                rankedDevices.insert({score, device});
        }

        if (rankedDevices.empty())
            return VK_NULL_HANDLE;

        // Return the highest scored device
        return rankedDevices.rbegin()->second;
    }

    uint32_t PhysicalDevice::EnumeratePhysicalDevice(const VkPhysicalDevice &device)
    {
        // Check required extensions
        uint32_t extensionPropertyCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropertyCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropertyCount,
                                             extensionProperties.data());

        // Verify all required extensions are present
        for (const char *requiredExtension : LogicalDevice::DeviceExtensions)
        {
            bool found = false;
            for (const auto &availableExtension : extensionProperties)
            {
                if (strcmp(requiredExtension, availableExtension.extensionName) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return 0; // Missing required extension
        }

        // Get device properties and features
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // Query Vulkan 1.2+ features for modern capabilities
        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk12Features.pNext = &vk13Features;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &vk12Features;
        vkGetPhysicalDeviceFeatures2(device, &features2);

#ifdef SF_DEBUG
        LogVulkanDevice(deviceProperties, extensionProperties);
#endif

        uint32_t score = 0;

        // Device type scoring (higher is better)
        switch (deviceProperties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 10000; // Dedicated GPU is best
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 1000; // Integrated GPU is second best
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 500; // Virtual GPU is acceptable
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 100; // CPU rendering is last resort
            break;
        default:
            score += 50;
            break;
        }

        // Vulkan API version bonus
        uint32_t apiMajor = VK_VERSION_MAJOR(deviceProperties.apiVersion);
        uint32_t apiMinor = VK_VERSION_MINOR(deviceProperties.apiVersion);

        if (apiMajor >= 1)
        {
            if (apiMinor >= 3)
                score += 300; // Vulkan 1.3+
            else if (apiMinor >= 2)
                score += 200; // Vulkan 1.2
            else if (apiMinor >= 1)
                score += 100; // Vulkan 1.1
        }

        // Modern feature bonuses
        if (vk12Features.timelineSemaphore)
            score += 50;
        if (vk12Features.descriptorIndexing)
            score += 50;
        if (vk12Features.bufferDeviceAddress)
            score += 50;
        if (vk13Features.dynamicRendering)
            score += 50;
        if (vk13Features.synchronization2)
            score += 50;

        // Memory size bonus (more VRAM is better)
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(device, &memProps);

        VkDeviceSize totalVRAM = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
        {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                totalVRAM += memProps.memoryHeaps[i].size;
        }

        // Add score based on GB of VRAM (capped at reasonable amount)
        score += static_cast<uint32_t>(std::min(totalVRAM / (1024 * 1024 * 1024), 16ULL) * 10);

        // Texture size support
        score += deviceProperties.limits.maxImageDimension2D / 1000;

        // Compute shader support bonus
        if (deviceProperties.limits.maxComputeWorkGroupCount[0] > 0)
            score += 20;

        // Geometry and tessellation shader support
        if (deviceFeatures.geometryShader)
            score += 10;
        if (deviceFeatures.tessellationShader)
            score += 10;

        // Essential feature requirements
        if (!deviceFeatures.samplerAnisotropy)
            score /= 2; // Heavily penalize but don't disqualify

        return score;
    }

    VkSampleCountFlagBits PhysicalDevice::GetMaxUsableSampleCount() const
    {
        auto counts = std::min(properties.limits.framebufferColorSampleCounts,
                               properties.limits.framebufferDepthSampleCounts);

        for (const auto &sampleFlag : STAGE_FLAG_BITS)
        {
            if (counts & sampleFlag)
                return sampleFlag;
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    void PhysicalDevice::LogVulkanDevice(
        const VkPhysicalDeviceProperties &deviceProperties,
        const std::vector<VkExtensionProperties> &extensionProperties)
    {
        std::stringstream ss;

        // Device type
        switch (deviceProperties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            ss << "Integrated";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            ss << "Discrete";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            ss << "Virtual";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            ss << "CPU";
            break;
        default:
            ss << "Other (" << deviceProperties.deviceType << ")";
            break;
        }

        ss << " Physical Device: " << deviceProperties.deviceID;

        // Vendor
        switch (deviceProperties.vendorID)
        {
        case 0x8086:
            ss << " \"Intel\"";
            break;
        case 0x10DE:
            ss << " \"NVIDIA\"";
            break;
        case 0x1002:
            ss << " \"AMD\"";
            break;
        case 0x13B5:
            ss << " \"ARM\"";
            break;
        case 0x5143:
            ss << " \"Qualcomm\"";
            break;
        default:
            ss << " \"0x" << std::hex << deviceProperties.vendorID << std::dec << "\"";
            break;
        }

        ss << " \"" << deviceProperties.deviceName << "\"\n";

        // API Version
        uint32_t apiMajor = VK_VERSION_MAJOR(deviceProperties.apiVersion);
        uint32_t apiMinor = VK_VERSION_MINOR(deviceProperties.apiVersion);
        uint32_t apiPatch = VK_VERSION_PATCH(deviceProperties.apiVersion);
        ss << "API Version: " << apiMajor << "." << apiMinor << "." << apiPatch << '\n';

        // Driver Version (vendor-specific interpretation)
        ss << "Driver Version: ";
        if (deviceProperties.vendorID == 0x10DE) // NVIDIA
        {
            ss << ((deviceProperties.driverVersion >> 22) & 0x3ff) << "."
               << ((deviceProperties.driverVersion >> 14) & 0xff) << "."
               << ((deviceProperties.driverVersion >> 6) & 0xff) << "."
               << (deviceProperties.driverVersion & 0x3f) << '\n';
        }
        else // Standard Vulkan version format
        {
            ss << VK_VERSION_MAJOR(deviceProperties.driverVersion) << "."
               << VK_VERSION_MINOR(deviceProperties.driverVersion) << "."
               << VK_VERSION_PATCH(deviceProperties.driverVersion) << '\n';
        }

        ss << "Extensions: ";
        for (size_t i = 0; i < extensionProperties.size(); i++)
        {
            ss << extensionProperties[i].extensionName;
            if (i < extensionProperties.size() - 1)
                ss << ", ";
        }
        ss << "\n\n";

        // Use fmt::runtime to prevent constexpr evaluation
        Log::Out(fmt::runtime("{}"), ss.str());
    }

    void PhysicalDevice::LogDeviceInfo() const
    {
        std::stringstream ss;
        ss << "Device Limits:\n";
        ss << "  Max Image Dimension 2D: " << properties.limits.maxImageDimension2D << '\n';
        ss << "  Max Image Dimension 3D: " << properties.limits.maxImageDimension3D << '\n';
        ss << "  Max Sampler Anisotropy: " << properties.limits.maxSamplerAnisotropy << '\n';
        ss << "  Max Viewports: " << properties.limits.maxViewports << '\n';
        ss << "  Max Compute Work Group Invocations: "
           << properties.limits.maxComputeWorkGroupInvocations << '\n';
        ss << "  Max MSAA Samples: ";

        switch (msaaSamples)
        {
        case VK_SAMPLE_COUNT_64_BIT:
            ss << "64x";
            break;
        case VK_SAMPLE_COUNT_32_BIT:
            ss << "32x";
            break;
        case VK_SAMPLE_COUNT_16_BIT:
            ss << "16x";
            break;
        case VK_SAMPLE_COUNT_8_BIT:
            ss << "8x";
            break;
        case VK_SAMPLE_COUNT_4_BIT:
            ss << "4x";
            break;
        case VK_SAMPLE_COUNT_2_BIT:
            ss << "2x";
            break;
        default:
            ss << "1x";
            break;
        }
        ss << '\n';

        // Memory info
        ss << "\nMemory Heaps:\n";
        for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++)
        {
            VkDeviceSize sizeInMB = memoryProperties.memoryHeaps[i].size / (1024 * 1024);
            ss << "  Heap " << i << ": " << sizeInMB << " MB";

            if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                ss << " (Device Local)";
            ss << '\n';
        }

        // Modern features
        ss << "\nVulkan 1.2 Features:\n";
        ss << "  Timeline Semaphore: " << (vulkan12Features.timelineSemaphore ? "Yes" : "No")
           << '\n';
        ss << "  Descriptor Indexing: " << (vulkan12Features.descriptorIndexing ? "Yes" : "No")
           << '\n';
        ss << "  Buffer Device Address: " << (vulkan12Features.bufferDeviceAddress ? "Yes" : "No")
           << '\n';
        ss << "  Scalar Block Layout: " << (vulkan12Features.scalarBlockLayout ? "Yes" : "No")
           << '\n';

        ss << "\nVulkan 1.3 Features:\n";
        ss << "  Dynamic Rendering: " << (vulkan13Features.dynamicRendering ? "Yes" : "No") << '\n';
        ss << "  Synchronization2: " << (vulkan13Features.synchronization2 ? "Yes" : "No") << '\n';
        ss << "  Maintenance4: " << (vulkan13Features.maintenance4 ? "Yes" : "No") << '\n';

        // Use fmt::runtime to prevent constexpr evaluation
        Log::Out(fmt::runtime("{}"), ss.str());
    }
}