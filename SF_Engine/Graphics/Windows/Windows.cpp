#include "Windows.hpp"

#include <algorithm>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

namespace SF::Engine
{
    void CallbackError(int32_t error, const char *description)
    {
        WindowManager::CheckGlfw(error);
        Log::Error("GLFW Error ", error, ": ", description);
    }

    void CallbackMonitor(GLFWmonitor *glfwMonitor, int32_t event)
    {
        auto &monitors = WindowManager::Get()->monitors;

        if (event == GLFW_CONNECTED)
        {
            auto monitor = monitors.emplace_back(std::make_unique<Monitor>(glfwMonitor)).get();
            WindowManager::Get()->onMonitorConnect(monitor, true);
        }
        else if (event == GLFW_DISCONNECTED)
        {
            for (const auto &monitor : monitors)
            {
                if (monitor->GetMonitor() == glfwMonitor)
                {
                    WindowManager::Get()->onMonitorConnect(monitor.get(), false);
                }
            }

            monitors.erase(std::remove_if(monitors.begin(), monitors.end(),
                                          [glfwMonitor](const auto &monitor)
                                          { return glfwMonitor == monitor->GetMonitor(); }));
        }
    }

    WindowManager::WindowManager()
    {
        // Set the error error callback
        glfwSetErrorCallback(CallbackError);

        // Initialize the GLFW library.
        if (glfwInit() == GLFW_FALSE)
            throw std::runtime_error("GLFW failed to initialize");

        // Checks Vulkan support on GLFW.
        if (glfwVulkanSupported() == GLFW_FALSE)
            throw std::runtime_error("GLFW failed to find Vulkan support");

        // Set the monitor callback
        glfwSetMonitorCallback(CallbackMonitor);

        // The window will stay hidden until after creation.
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        // Disable context creation.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Fixes 16 bit stencil bits in macOS.
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        // No stereo view!
        glfwWindowHint(GLFW_STEREO, GLFW_FALSE);

        // Get connected monitors.
        int32_t monitorCount;
        auto monitors = glfwGetMonitors(&monitorCount);

        for (uint32_t i = 0; i < static_cast<uint32_t>(monitorCount); i++)
            this->monitors.emplace_back(std::make_unique<Monitor>(monitors[i]));
    }

    WindowManager::~WindowManager()
    {
        // Destroy all windows BEFORE glfwTerminate : calling glfwDestroyWindow
        // after glfwTerminate fires a GLFW error
        windows.clear();
        glfwTerminate();
    }

    void WindowManager::Update()
    {
        glfwPollEvents();
        for (auto &window : windows)
            window->Update();
    }

    Window *WindowManager::AddWindow()
    {
        auto window = windows.emplace_back(std::make_unique<Window>(windows.size())).get();
        onAddWindow(window, true);
        return window;
    }

    const Window *WindowManager::GetWindow(WindowId id) const
    {
        if (id >= windows.size())
            return nullptr;
        return windows.at(id).get();
    }

    Window *WindowManager::GetWindow(WindowId id)
    {
        if (id >= windows.size())
            return nullptr;
        return windows.at(id).get();
    }

    const Monitor *WindowManager::GetPrimaryMonitor() const
    {
        for (const auto &monitor : monitors)
        {
            if (monitor->IsPrimary())
                return monitor.get();
        }
        return nullptr;
    }

    void WindowManager::CheckGlfw(int32_t result)
    {
        if (result)
            return;

        Log::Error("GLFW error:  ", result, '\n');
    }

    std::pair<const char **, uint32_t> WindowManager::GetInstanceExtensions() const
    {
        uint32_t glfwExtensionCount;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        return std::make_pair(glfwExtensions, glfwExtensionCount);
    }
}