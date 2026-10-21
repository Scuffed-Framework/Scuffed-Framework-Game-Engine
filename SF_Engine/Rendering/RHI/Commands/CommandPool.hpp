#pragma once

#include <thread>
#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif

#include <UtilityClasses/Export.hpp>
#include <volk.h>

namespace SF::Engine
{
    /**
     * @brief Class that represents a command pool.
     */
    class CommandPool
    {
    public:
        explicit CommandPool(const std::thread::id &threadId = std::this_thread::get_id());

        ~CommandPool();

        operator const VkCommandPool &() const { return commandPool; }

        const VkCommandPool &GetCommandPool() const { return commandPool; }
        const std::thread::id &GetThreadId() const { return threadId; }

    private:
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::thread::id threadId;
    };
} // namespace SF::Engine
