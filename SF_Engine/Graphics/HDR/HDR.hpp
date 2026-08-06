#pragma once
#include <volk.h>
#include <Platform/PlatformIncludes.hpp>

namespace SF::Engine
{
    VkSurfaceFormatKHR SelectHDRFormat(const std::vector<VkSurfaceFormatKHR> availFormats, OperatingSystem OS)
    {
        if(OS == OperatingSystem::Windows_Or_Xbox)
        {
            for(const auto& format : availFormats){
                if(formant.format == VK_FORMAT_R16G16B16A16_SFLOAT && format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) return format;
            }
        }
        for(const auto& format : availFormats){
                if(formant.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) return format;
        }
        for(const auto& format : availFormats){
                if(formant.colorSpace == VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT && format.colorSpace == VK_COLOR_SPACE_BT2020_LINEAR_EXT) return format;
        }
        return availFormats[0];
    }
}