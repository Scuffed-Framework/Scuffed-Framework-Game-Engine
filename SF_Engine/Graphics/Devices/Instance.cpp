#include "Instance.hpp"

#include <fmt/format.h>

#include <Graphics/RenderSystem.hpp>
#include <Graphics/Windows/WindowManager.hpp>

#define VK_EXT_DEBUG_UTILS_EXTENSION_NAME "VK_EXT_debug_utils"

namespace SF::Engine
{
    const std::vector<const char *> Instance::ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"}; // "VK_LAYER_RENDERDOC_Capture"

    VKAPI_ATTR VkBool32 VKAPI_CALL
    CallbackDebug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        // Store message in a local variable to avoid lifetime issues with fmt constexpr evaluation
        const char *msg = pCallbackData->pMessage;

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            Log::Warning(fmt::runtime("{}"), msg);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            Log::Info(fmt::runtime("{}"), msg);
        else // VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            Log::Error(fmt::runtime("{}"), msg);

        return VK_FALSE;
    }

    VkResult Instance::CreateDebugMessenger(VkInstance instance,
                                            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkDebugUtilsMessengerEXT *pDebugMessenger)
    {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (func)
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void Instance::DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                         const VkAllocationCallbacks *pAllocator)
    {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func)
            return func(instance, messenger, pAllocator);
    }

    void Instance::FilePushDescriptorSet(VkDevice device, VkCommandBuffer commandBuffer,
                                         VkPipelineBindPoint pipelineBindPoint,
                                         VkPipelineLayout layout, uint32_t set,
                                         uint32_t descriptorWriteCount,
                                         const VkWriteDescriptorSet *pDescriptorWrites)
    {
        auto func = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
            vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));
        if (func)
            func(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount,
                 pDescriptorWrites);
    }

    uint32_t Instance::FindMemoryTypeIndex(
        const VkPhysicalDeviceMemoryProperties *deviceMemoryProperties,
        const VkMemoryRequirements *memoryRequirements, VkMemoryPropertyFlags requiredProperties)
    {
        for (uint32_t i = 0; i < deviceMemoryProperties->memoryTypeCount; ++i)
        {
            if (memoryRequirements->memoryTypeBits & (1 << i))
            {
                if ((deviceMemoryProperties->memoryTypes[i].propertyFlags & requiredProperties) ==
                    requiredProperties)
                {
                    return i;
                }
            }
        }

        throw std::runtime_error("Couldn't find a proper memory type");
    }

    Instance::Instance()
    {
#if defined(Debug) || defined(_DEBUG)
        validationLayersEnabled = true;
#endif

        CreateInstance();
        CreateDebugMessenger();
    }

    Instance::~Instance()
    {
        DestroyDebugMessenger(instance, debugMessenger, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    bool Instance::CheckValidationLayerSupport() const
    {
        uint32_t instanceLayerPropertyCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerPropertyCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount,
                                           instanceLayerProperties.data());

#ifdef Debug
        LogVulkanLayers(instanceLayerProperties);
#endif

        for (const auto &layerName : ValidationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : instanceLayerProperties)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                Log::Error(fmt::runtime("Vulkan validation layer not found: \"{}\""), layerName);
                return false;
            }
        }

        return true;
    }

    std::vector<const char *> Instance::GetExtensions() const
    {
        // Sets up the extensions.
        auto [glfwExtensions, glfwExtensionsCount] = WindowManager::Get()->GetInstanceExtensions();

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);

        if (validationLayersEnabled)
            extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        // extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        return extensions;
    }

    void Instance::CreateInstance()
    {
        const auto &engineVersion = Engine::Get()->GetVersion();
        // const auto &appVersion = Engine::Get()->GetApp()->GetVersion();
        // const auto &appName = Engine::Get()->GetApp()->GetName();

        RenderSystem::CheckVkResult(volkInitialize());

        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        // applicationInfo.pApplicationName = appName.c_str();
        // applicationInfo.applicationVersion = VK_MAKE_VERSION(appVersion.major, appVersion.minor,
        // appVersion.patch);
        applicationInfo.pEngineName = "SF Engine";
        applicationInfo.engineVersion =
            VK_MAKE_VERSION(engineVersion.major, engineVersion.minor, engineVersion.patch);
        applicationInfo.apiVersion = VK_API_VERSION_1_3;

        if (validationLayersEnabled && !CheckValidationLayerSupport())
        {
            Log::Error("Validation layers requested, but not available!\n");
            validationLayersEnabled = false;
        }

        auto extensions = GetExtensions();

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};

        if (validationLayersEnabled)
        {
            debugUtilsMessengerCreateInfo.sType =
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsMessengerCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugUtilsMessengerCreateInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugUtilsMessengerCreateInfo.pfnUserCallback = &CallbackDebug;
            instanceCreateInfo.pNext =
                static_cast<VkDebugUtilsMessengerCreateInfoEXT *>(&debugUtilsMessengerCreateInfo);

            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = ValidationLayers.data();
        }

        RenderSystem::CheckVkResult(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

        volkLoadInstanceOnly(instance);
    }

    void Instance::CreateDebugMessenger()
    {
        if (!validationLayersEnabled)
            return;

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
        debugUtilsMessengerCreateInfo.sType =
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMessengerCreateInfo.pfnUserCallback = &CallbackDebug;
        RenderSystem::CheckVkResult(CreateDebugMessenger(instance, &debugUtilsMessengerCreateInfo,
                                                         nullptr, &debugMessenger));
    }

    void Instance::LogVulkanLayers(const std::vector<VkLayerProperties> &layerProperties)
    {
        std::stringstream ss;
        ss << "Instance Layers: ";
        for (const auto &layer : layerProperties)
            ss << layer.layerName << ", ";
        ss << "\n\n";

        // Use fmt::runtime to prevent constexpr evaluation
        Log::Out(fmt::runtime("{}"), ss.str());
    }
}