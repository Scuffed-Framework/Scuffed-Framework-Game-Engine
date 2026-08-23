#pragma once

#include <Engine/Log/Log.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace SF::Engine
{
    // Thrown specifically for VK_ERROR_DEVICE_LOST so callers can catch it
    // separately from other Vulkan failures.
    class DeviceLostException : public std::runtime_error
    {
    public:
        explicit DeviceLostException(const std::string &context)
            : std::runtime_error("Vulkan device lost: " + context)
        {
        }
    };

    // Subsystems holding device-local resources (pipelines, buffers, images,
    // descriptor sets) register a lost/restored pair here so RenderSystem's
    // recovery path doesn't need to know what they are. Mirrors your existing
    // ModuleRegistrar / function-local-static singleton pattern.
    class DeviceRecoveryRegistrar
    {
    public:
        using Callback = std::function<void()>;

        static DeviceRecoveryRegistrar &Get()
        {
            static DeviceRecoveryRegistrar instance;
            return instance;
        }

        void Register(std::string name, Callback onLost, Callback onRestored)
        {
            entries.push_back({std::move(name), std::move(onLost), std::move(onRestored)});
        }

        // Reverse registration order — most-dependent resources release first,
        // same convention as normal shutdown.
        void NotifyDeviceLost()
        {
            for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            {
                Log::Info("[DeviceRecovery] Releasing: {}", it->name);
                it->onLost();
            }
        }

        void NotifyDeviceRestored()
        {
            for (auto &e : entries)
            {
                Log::Out("[DeviceRecovery] Restoring: {}", e.name);
                e.onRestored();
            }
        }

    private:
        struct Entry
        {
            std::string name;
            Callback onLost;
            Callback onRestored;
        };
        std::vector<Entry> entries;
    };
}