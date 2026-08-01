#include "Monitor.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include "WindowManager.hpp"

namespace SF::Engine
{
    Monitor::Monitor(GLFWmonitor *monitor) : monitor(monitor) {}

    bool Monitor::IsPrimary() const
    {
        return monitor == glfwGetPrimaryMonitor();
    }

    Vector2Uint Monitor::GetWorkareaSize() const
    {
        int32_t width;
        int32_t height;
        glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &width, &height);
        return {width, height};
    }

    Vector2Uint Monitor::GetWorkareaPosition() const
    {
        int32_t xPos;
        int32_t yPos;
        glfwGetMonitorWorkarea(monitor, &xPos, &yPos, nullptr, nullptr);
        return Vector2Uint(xPos, yPos);
    }

    Vector2Uint Monitor::GetSize() const
    {
        int32_t widthMM;
        int32_t heightMM;
        glfwGetMonitorPhysicalSize(monitor, &widthMM, &heightMM);
        return {widthMM, heightMM};
    }

    Vector2float Monitor::GetContentScale() const
    {
        float xScale;
        float yScale;
        glfwGetMonitorContentScale(monitor, &xScale, &yScale);
        return {xScale, yScale};
    }

    Vector2Uint Monitor::GetPosition() const
    {
        int32_t xpos;
        int32_t ypos;
        glfwGetMonitorPos(monitor, &xpos, &ypos);
        return {xpos, ypos};
    }

    std::string Monitor::GetName() const
    {
        return glfwGetMonitorName(monitor);
    }

    std::vector<VideoMode> Monitor::GetVideoModes() const
    {
        int32_t videoModeCount;
        auto videoModes = glfwGetVideoModes(monitor, &videoModeCount);
        std::vector<VideoMode> modes(static_cast<uint32_t>(videoModeCount));
        for (uint32_t i = 0; i < static_cast<uint32_t>(videoModeCount); i++)
            modes[i] = *reinterpret_cast<const VideoMode *>(&videoModes[i]);
        return modes;
    }

    VideoMode Monitor::GetVideoMode() const
    {
        auto videoMode = glfwGetVideoMode(monitor);
        return *reinterpret_cast<const VideoMode *>(videoMode);
    }

    GammaRamp Monitor::GetGammaRamp() const
    {
        auto gamaRamp = glfwGetGammaRamp(monitor);
        return *reinterpret_cast<const GammaRamp *>(gamaRamp);
    }

    void Monitor::SetGammaRamp(const GammaRamp &gammaRamp) const
    {
        auto ramp = reinterpret_cast<const GLFWgammaramp *>(&gammaRamp);
        glfwSetGammaRamp(monitor, ramp);
    }
}