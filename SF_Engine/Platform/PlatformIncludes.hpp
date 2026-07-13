#pragma once

// TODO: Remove
namespace SF::Engine
{
    enum SupportedPlatforms
    {
        Linux,
        Apple,
        Windows
        // XBOX impl directx12 & choosing a rendering API
    };

    enum RenderAPI
    {
        DirectX12, // supports: Microsoft Windows, XBox
        MoltenVk,  // supports: Apple Macbook and I-devices
        Vulkan,    // supports: Microsoft Windows, Linux
    };

    struct EngineCompilationInfo
    {
#if defined _PLATFORM_WINDOWS || _WIN32
        SupportedPlatforms CompilationPlatform = SupportedPlatforms::Windows;
#elif defined _PLATFORM_MACOS || __APPLE__

        SupportedPlatforms CompilationPlatform = SupportedPlatforms::Apple;
#elif defined _PLATFORM_LINUX || __linux__

        SupportedPlatforms CompilationPlatform = SupportedPlatforms::Linux;
#elif defined _PLATFORM_XBOX || __xbox__

        // SupportedPlatforms CompilationPlatform = SupportedPlatforms::Windows;
#else

#endif
    };
}