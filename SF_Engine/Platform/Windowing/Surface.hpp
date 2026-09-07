#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>

namespace SF::Engine
{
    class Instance;
    class LogicalDevice;
    class PhysicalDevice;
    class Window;

    class Surface
    {
        friend class RenderSystem;

    public:
        Surface(const Instance &instance, const PhysicalDevice &physicalDevice,
                const LogicalDevice &logicalDevice, const Window &window);
        ~Surface();

        operator const VkSurfaceKHR &() const
        {
            return surface;
        }

        const VkSurfaceKHR &GetSurface() const
        {
            return surface;
        }
        VkSurfaceCapabilitiesKHR GetCapabilities() const;
        const VkSurfaceFormatKHR &GetFormat() const
        {
            return format;
        }

    private:
        const Instance &instance;
        const PhysicalDevice &physicalDevice;
        const LogicalDevice &logicalDevice;
        const Window &window;

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        mutable VkSurfaceCapabilitiesKHR capabilities = {}; // mutable: re-queried on every GetCapabilities() call
        VkSurfaceFormatKHR format = {};
    };
}