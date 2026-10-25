#pragma once
#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif

#include <vector>
#include <volk.h>

namespace SF::Engine
{
    using namespace std;
    class Instance
    {
        friend class RenderSystem;

    public:
        friend VKAPI_ATTR VkBool32 VKAPI_CALL CallbackDebug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                            void *pUserData);

        Instance();
        ~Instance();

        static VkResult CreateDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDebugUtilsMessengerEXT *pDebugMessenger);
        static void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                          const VkAllocationCallbacks *pAllocator);

        static void FilePushDescriptorSet(VkDevice device, VkCommandBuffer commandBuffer,
                                          VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set,
                                          uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites);

        static uint32_t FindMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties *deviceMemoryProperties,
                                            const VkMemoryRequirements *memoryRequirements,
                                            VkMemoryPropertyFlags requiredProperties);

        operator const VkInstance &() const { return instance; }

        [[nodiscard]] bool AreValidationLayersEnabled() const { return validationLayersEnabled; }
        [[nodiscard]] const VkInstance &GetInstance() const { return instance; }

        static const vector<const char *> ValidationLayers;

    private:
        [[nodiscard]] bool CheckValidationLayerSupport() const;
        [[nodiscard]] vector<const char *> GetExtensions() const;
        void CreateInstance();
        void CreateDebugMessenger();

        static void LogVulkanLayers(const vector<VkLayerProperties> &layerProperties);

    public:
        bool validationLayersEnabled = false;

    private:
        VkInstance instance                     = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    };
} // namespace SF::Engine
