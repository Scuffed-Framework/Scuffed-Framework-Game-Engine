#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <vector>

namespace SF::Engine
{
    class Instance
    {
        friend class RenderSystem;

    public:
        friend VKAPI_ATTR VkBool32 VKAPI_CALL
        CallbackDebug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

        Instance();
        ~Instance();

        static VkResult CreateDebugMessenger(VkInstance instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDebugUtilsMessengerEXT* pDebugMessenger);
        static void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                          const VkAllocationCallbacks* pAllocator);

        static void FilePushDescriptorSet(VkDevice device, VkCommandBuffer commandBuffer,
                                          VkPipelineBindPoint pipelineBindPoint,
                                          VkPipelineLayout layout, uint32_t set,
                                          uint32_t descriptorWriteCount,
                                          const VkWriteDescriptorSet* pDescriptorWrites);

        static uint32_t FindMemoryTypeIndex(
            const VkPhysicalDeviceMemoryProperties* deviceMemoryProperties,
            const VkMemoryRequirements* memoryRequirements,
            VkMemoryPropertyFlags requiredProperties);

        operator const VkInstance&() const
        {
            return instance;
        }

        bool AreValidationLayersEnabled() const
        {
            return validationLayersEnabled;
        }
        const VkInstance& GetInstance() const
        {
            return instance;
        }

        static const std::vector<const char*> ValidationLayers;

    private:
        bool CheckValidationLayerSupport() const;
        std::vector<const char*> GetExtensions() const;
        void CreateInstance();
        void CreateDebugMessenger();

        static void LogVulkanLayers(const std::vector<VkLayerProperties>& layerProperties);

    public:
        bool validationLayersEnabled = false;

    private:
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    };
}