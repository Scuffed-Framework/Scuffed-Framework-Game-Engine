#include "LogicalDevice.hpp"

#include <Rendering/RenderSystem.hpp>
#include <unordered_set>
#include "Instance.hpp"
#include "PhysicalDevice.hpp"

namespace SF::Engine
{
    const std::vector<const char *> LogicalDevice::DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    LogicalDevice::LogicalDevice(const Instance &instance, const PhysicalDevice &physicalDevice)
        : instance(instance), physicalDevice(physicalDevice)
    {
        CreateQueueIndices();
        CreateLogicalDevice();
    }

    LogicalDevice::~LogicalDevice()
    {
        if (logicalDevice != VK_NULL_HANDLE)
        {
            vkDestroyDevice(logicalDevice, nullptr);
            logicalDevice = VK_NULL_HANDLE;
        }
    }

    void LogicalDevice::CreateQueueIndices()
    {
        uint32_t deviceQueueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceQueueFamilyPropertyCount,
                                                 nullptr);
        std::vector<VkQueueFamilyProperties> deviceQueueFamilyProperties(
            deviceQueueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceQueueFamilyPropertyCount,
                                                 deviceQueueFamilyProperties.data());

        std::optional<uint32_t> GraphicsFamily, presentFamily, computeFamily, transferFamily;
        std::optional<uint32_t> dedicatedTransferFamily, dedicatedComputeFamily;

        for (uint32_t i = 0; i < deviceQueueFamilyPropertyCount; i++)
        {
            const auto &queueFamily = deviceQueueFamilyProperties[i];

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (!GraphicsFamily)
                {
                    GraphicsFamily = i;
                    this->GraphicsFamily = i;
                    supportedQueues |= VK_QUEUE_GRAPHICS_BIT;
                }
            }

            if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                if (!presentFamily)
                {
                    presentFamily = i;
                    this->presentFamily = i;
                }
            }

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                if (!computeFamily)
                {
                    computeFamily = i;
                    this->computeFamily = i;
                    supportedQueues |= VK_QUEUE_COMPUTE_BIT;
                }
                if (!dedicatedComputeFamily && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    dedicatedComputeFamily = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                if (!transferFamily)
                {
                    transferFamily = i;
                    this->transferFamily = i;
                    supportedQueues |= VK_QUEUE_TRANSFER_BIT;
                }
                if (!dedicatedTransferFamily && queueFamily.queueFlags == VK_QUEUE_TRANSFER_BIT)
                    dedicatedTransferFamily = i;
            }
        }

        if (dedicatedComputeFamily)
        {
            this->computeFamily = *dedicatedComputeFamily;
            Log::Info("Using dedicated compute queue family\n");
        }
        if (dedicatedTransferFamily)
        {
            this->transferFamily = *dedicatedTransferFamily;
            Log::Info("Using dedicated transfer queue family\n");
        }

        if (!GraphicsFamily)
            throw std::runtime_error("Failed to find queue family supporting VK_QUEUE_GRAPHICS_BIT");
        if (!presentFamily)
            throw std::runtime_error("Failed to find queue family supporting present");
        if (!computeFamily)
            Log::Warning("No compute queue family found\n");
        if (!transferFamily)
            Log::Warning("No transfer queue family found\n");
    }

    void LogicalDevice::CreateLogicalDevice()
    {
        std::unordered_set<uint32_t> uniqueQueueFamilies = {GraphicsFamily, presentFamily,
                                                            computeFamily, transferFamily};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;

        for (uint32_t qf : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = qf;
            info.queueCount = 1;
            info.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(info);
        }

        //  Query available features via Vulkan 1.1+ feature chain
        VkPhysicalDeviceVulkan13Features avail13 = {};
        avail13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceVulkan12Features avail12 = {};
        avail12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        avail12.pNext = &avail13;

        VkPhysicalDeviceVulkan11Features avail11 = {};
        avail11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        avail11.pNext = &avail12;

        VkPhysicalDeviceFeatures2 avail = {};
        avail.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        avail.pNext = &avail11;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &avail);

        auto &af = avail.features;

        //  Core 1.0 features
        VkPhysicalDeviceFeatures req = {};
        if (af.sampleRateShading)
            req.sampleRateShading = VK_TRUE;
        if (af.fillModeNonSolid)
            req.fillModeNonSolid = VK_TRUE;
        if (af.wideLines && af.fillModeNonSolid)
            req.wideLines = VK_TRUE;
        if (af.samplerAnisotropy)
            req.samplerAnisotropy = VK_TRUE;
        if (af.textureCompressionBC)
            req.textureCompressionBC = VK_TRUE;
        else if (af.textureCompressionASTC_LDR)
            req.textureCompressionASTC_LDR = VK_TRUE;
        else if (af.textureCompressionETC2)
            req.textureCompressionETC2 = VK_TRUE;
        if (af.vertexPipelineStoresAndAtomics)
            req.vertexPipelineStoresAndAtomics = VK_TRUE;
        if (af.fragmentStoresAndAtomics)
            req.fragmentStoresAndAtomics = VK_TRUE;
        if (af.shaderStorageImageExtendedFormats)
            req.shaderStorageImageExtendedFormats = VK_TRUE;
        if (af.shaderStorageImageWriteWithoutFormat)
            req.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        if (af.geometryShader)
            req.geometryShader = VK_TRUE;
        if (af.tessellationShader)
            req.tessellationShader = VK_TRUE;
        if (af.multiViewport)
            req.multiViewport = VK_TRUE;
        enabledFeatures = req;

        //  Vulkan 1.1
        VkPhysicalDeviceVulkan11Features req11 = {};
        req11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        if (avail11.shaderDrawParameters)
            req11.shaderDrawParameters = VK_TRUE;

        //  Vulkan 1.2
        VkPhysicalDeviceVulkan12Features req12 = {};
        req12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        req12.pNext = &req11;

        if (avail12.timelineSemaphore)
        {
            req12.timelineSemaphore = VK_TRUE;
            Log::Info("Enabling timeline semaphores\n");
        }

        if (avail12.descriptorIndexing)
        {
            req12.descriptorIndexing = VK_TRUE;
            if (avail12.shaderSampledImageArrayNonUniformIndexing)
                req12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            if (avail12.runtimeDescriptorArray)
                req12.runtimeDescriptorArray = VK_TRUE;
            if (avail12.descriptorBindingPartiallyBound)
                req12.descriptorBindingPartiallyBound = VK_TRUE;
            if (avail12.descriptorBindingVariableDescriptorCount)
                req12.descriptorBindingVariableDescriptorCount = VK_TRUE;
            // Required for vkUpdateDescriptorSets while command buffers are pending
            if (avail12.descriptorBindingUniformBufferUpdateAfterBind)
                req12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
            if (avail12.descriptorBindingStorageBufferUpdateAfterBind)
                req12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
            if (avail12.descriptorBindingSampledImageUpdateAfterBind)
                req12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            if (avail12.descriptorBindingStorageImageUpdateAfterBind)
                req12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
            Log::Info("Enabling descriptor indexing features\n");
        }

        if (avail12.bufferDeviceAddress)
        {
            req12.bufferDeviceAddress = VK_TRUE;
            Log::Info("Enabling buffer device address\n");
        }
        if (avail12.scalarBlockLayout)
        {
            req12.scalarBlockLayout = VK_TRUE;
            Log::Info("Enabling scalar block layout\n");
        }
        if (avail12.hostQueryReset)
            req12.hostQueryReset = VK_TRUE;

        //  Vulkan 1.3
        VkPhysicalDeviceVulkan13Features req13 = {};
        req13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        req13.pNext = &req12;

        if (avail13.dynamicRendering)
        {
            req13.dynamicRendering = VK_TRUE;
            Log::Info("Enabling dynamic rendering\n");
        }
        if (avail13.synchronization2)
        {
            req13.synchronization2 = VK_TRUE;
            Log::Info("Enabling synchronization2\n");
        }

        // Core 1.0 features go at the tail of the pNext chain via Features2
        VkPhysicalDeviceFeatures2 req2 = {};
        req2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        req2.features = req;
        req11.pNext = &req2; // tail of chain

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &req13;
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();
        deviceCreateInfo.pEnabledFeatures = nullptr; // using pNext chain instead

        Log::Info("Calling vkCreateDevice");
        RenderSystem::CheckVkResult(
            vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice));
        Log::Info("vkCreateDevice succeeded");

        volkLoadDevice(logicalDevice);

        vkGetDeviceQueue(logicalDevice, GraphicsFamily, 0, &GraphicsQueue);
        vkGetDeviceQueue(logicalDevice, presentFamily, 0, &presentQueue);
        vkGetDeviceQueue(logicalDevice, computeFamily, 0, &computeQueue);
        vkGetDeviceQueue(logicalDevice, transferFamily, 0, &transferQueue);
    }
}
