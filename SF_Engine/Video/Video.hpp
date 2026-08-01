#pragma once
#include <Engine/Module.hpp>
#include <volk.h>
#include <vulkan/vulkan_video.hpp
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    class Video : public ModuleRegistrar<Video>
    {
        inline static const bool registered = Register(Stage::Pre);

    public:
        Video();
        ~Video();

        bool Initialize() override
        {
            // vkGetPhysicalDeviceVideoCapabilitiesKHR(RenderSystem::Get()->GetPhysicalDevice()->GetPhysicalDevice(), );
        }

        VkResult QuerryHardwareVideoCapabilities(const VkVideoProfileInfoKHR *VideoProfile, VkVideoCapabilitiesKHR *VideoCapabilities)
        {
            return vkGetPhysicalDeviceVideoCapabilitiesKHR(RenderSystem::Get()->GetPhysicalDevice()->GetPhysicalDevice(), VideoProfile, VideoCapabilities);
        }
    };
}