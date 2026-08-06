#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)

#define NOMINMAX
#include <Windows.h>

namespace SF::Engine::Platform
{
    using NativePID = DWORD;
    using NativeProcessHandle = HANDLE;
    constexpr NativePID InvalidPID = 0;
    constexpr NativeProcessHandle InvalidProcessHandle = nullptr;
}

#elif defined(__linux__) || defined(__APPLE__)

#include <sys/types.h>

namespace SF::Engine::Platform
{
    using NativePID = pid_t;
    using NativeProcessHandle = pid_t; // Processes are identified by PID.
    constexpr NativePID InvalidPID = -1;
    constexpr NativeProcessHandle InvalidProcessHandle = -1;
}

#else
#error Unsupported platform.
#endif

#if defined(_WIN32)

namespace SF::Engine
{
    inline std::filesystem::path GetExecutablePath()
    {
        std::vector<wchar_t> buffer(MAX_PATH);

        while (true)
        {
            DWORD len = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));

            if (len == 0)
                throw std::runtime_error("GetModuleFileNameW failed");

            if (len < buffer.size() - 1)
                return std::filesystem::path(buffer.data(), buffer.data() + len);

            buffer.resize(buffer.size() * 2);
        }
    }
}

#elif defined(__linux__)

#include <unistd.h>

namespace SF::Engine
{
    inline std::filesystem::path GetExecutablePath()
    {
        std::vector<char> buffer(4096);

        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size());

        if (len == -1)
            throw std::runtime_error("readlink failed");

        return std::filesystem::path(std::string(buffer.data(), static_cast<size_t>(len)));
    }
}

#elif defined(__APPLE__)

#include <mach-o/dyld.h>

namespace SF::Engine
{
    inline std::filesystem::path GetExecutablePath()
    {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size); // First call just gets required size.

        std::vector<char> buffer(size);

        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            throw std::runtime_error("_NSGetExecutablePath failed");

        return std::filesystem::weakly_canonical(buffer.data());
    }
}

#else
#error Unsupported platform.
#endif

namespace SF::Engine
{
    enum class SupportedPlatform
    {
        Linux,
        Apple,
        Windows,
        Xbox
        // XBOX impl: DirectX12 + platform-specific rendering path.
    };

    enum class RenderAPI
    {
        DirectX12, // Windows, Xbox
        MoltenVk,  // macOS, iOS
        Vulkan,    // Windows, Linux
    };

    struct EngineCompilationInfo
    {
#if defined(_PLATFORM_XBOX) || defined(__xbox__)
        static constexpr SupportedPlatform CompilationPlatform = SupportedPlatform::Xbox;
#elif defined(_PLATFORM_WINDOWS) || defined(_WIN32)
        static constexpr SupportedPlatform CompilationPlatform = SupportedPlatform::Windows;
#elif defined(_PLATFORM_MACOS) || defined(__APPLE__)
        static constexpr SupportedPlatform CompilationPlatform = SupportedPlatform::Apple;
#elif defined(_PLATFORM_LINUX) || defined(__linux__)
        static constexpr SupportedPlatform CompilationPlatform = SupportedPlatform::Linux;
#else
#error Unsupported platform.
#endif
    };

    enum OperatingSystem
    {
        Windows_Or_Xbox,
        Linux,
        MacOs,
        IOS,
        Android,
    };
}