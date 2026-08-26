#pragma once

#include <Engine/Module.hpp>
#include "Commands/CommandBuffer.hpp"
#include "Commands/CommandPool.hpp"
#include "Devices/Instance.hpp"
#include "Devices/LogicalDevice.hpp"
#include "Devices/PhysicalDevice.hpp"
#include "Renderer.hpp"
#include "Windows/Surface.hpp"
#include "Windows/WindowManager.hpp"
#include "Bindless/Bindless.hpp"

#include <UtilityClasses/NoCopy.hpp>
#include <Delegates/MultiCastDelegate.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <thread>
#include <variant>

// need more vma?

namespace SF::Engine
{
    /**
     * @brief Module that manages the Vulkan instance, devices, surfaces, and rendering
     * infrastructure.
     */
    class BindlessManager;

    class RenderSystem final : public ModuleRegistrar<RenderSystem>
    {
        friend class ModuleRegistrar<RenderSystem>;
        REGISTER_MODULE(RenderSystem, Module::Stage::Render, Module::Requires<WindowManager>{});

    public:
        RenderSystem();
        ~RenderSystem() override;

        void PostInit();
        void PreShutdown();

        // Module interface implementation
        void Update() override;

        Module::Stage GetStage() const override
        {
            return Module::Stage::Render;
        }
        TypeId GetTypeId() const override
        {
            return TypeInfo<Module>::GetTypeId<RenderSystem>();
        }
        std::string_view GetName() const override
        {
            return "RenderSystem";
        }

        /**
         * @brief Convert Vulkan result to string (for debugging)
         */
        static std::string StrVkResult(VkResult result);

        /**
         * @brief Check Vulkan result and throw on error
         */
        static void CheckVkResult(VkResult result);

        /**
         * @brief Takes a screenshot of the current swapchain image
         */
        void CaptureScreenshot(const std::filesystem::path &filename,
                               std::size_t surfaceId = 0) const;

        /**
         * @brief Get or create command pool for current thread
         */
        const std::shared_ptr<CommandPool> &GetCommandPool(
            const std::thread::id &threadId = std::this_thread::get_id());

        /**
         * @brief Get render stage by index
         */
        const RenderStage *GetRenderStage(uint32_t index) const;

        /**
         * @brief Get attachment descriptor by name
         */
        const Descriptor *GetAttachment(const std::string &name) const;

        // Device and resource accessors
        const Instance *GetInstance() const noexcept
        {
            return instance.get();
        }
        const PhysicalDevice *GetPhysicalDevice() const noexcept
        {
            return physicalDevice.get();
        }
        const LogicalDevice *GetLogicalDevice() const noexcept
        {
            return logicalDevice.get();
        }
        VkPipelineCache GetPipelineCache() const noexcept
        {
            return pipelineCache;
        }

        /**
         * @brief Get surface by index
         */
        const Surface *GetSurface(std::size_t id) const noexcept
        {
            return id < surfaces.size() ? surfaces[id].get() : nullptr;
        }

        /**
         * @brief Get swapchain by index
         */
        const Swapchain *GetSwapchain(std::size_t id) const noexcept
        {
            return id < swapchains.size() ? swapchains[id].get() : nullptr;
        }

        void SetFramebufferResized(std::size_t id) const
        {
            if (id < perSurfaceBuffers.size() && perSurfaceBuffers[id])
                perSurfaceBuffers[id]->framebufferResized = true;
        }

        /**
         * @brief Get number of surfaces
         */
        std::size_t GetSurfaceCount() const noexcept
        {
            return surfaces.size();
        }

        void SetRenderer(std::unique_ptr<Renderer> &&r)
        {
            renderer = std::move(r);
        }

        Renderer *GetRenderer() const noexcept
        {
            return renderer.get();
        }

        BindlessManager *GetBindlessManager() const { return bindlessMgr.get(); }

        /**
         * @brief Rebuild render stages, swapchain, and framebuffers.
         * Safe to call from Stage::Normal (outside the render loop).
         */
        void ResetRenderStages();

        uint32_t GetVkAPIVersion();
    private:
        /**
         * @brief Per-surface synchronization and command buffers
         */
        struct PerSurfaceBuffers
        {
            std::vector<VkSemaphore> presentCompletes;
            std::vector<VkSemaphore> renderCompletes;
            std::vector<VkFence> flightFences;
            std::vector<std::unique_ptr<CommandBuffer>> commandBuffers;

            std::size_t currentFrame = 0;
            bool framebufferResized = false;
        };

        // Helper to enumerate with index
        template <typename Container>
        static auto Enumerate(Container &container)
        {
            struct Iterator
            {
                std::size_t index;
                typename Container::iterator iter;

                auto operator*()
                {
                    return std::make_pair(index, std::ref(*iter));
                }
                Iterator &operator++()
                {
                    ++index;
                    ++iter;
                    return *this;
                }
                bool operator!=(const Iterator &other) const
                {
                    return iter != other.iter;
                }
            };

            struct EnumerateWrapper
            {
                Container &cont;
                auto begin()
                {
                    return Iterator{0, cont.begin()};
                }
                auto end()
                {
                    return Iterator{0, cont.end()};
                }
            };

            return EnumerateWrapper{container};
        }

        // Initialization helpers
        void CreatePipelineCache();

        // Render loop helpers
        void RecreateSwapchain();
        void RecreateCommandBuffers(std::size_t surfaceId);
        void RecreatePass(std::size_t surfaceId, RenderStage &renderStage);
        void RecreateAttachmentsMap();

        bool StartRenderpass(std::size_t surfaceId, RenderStage &renderStage);
        void EndRenderpass(std::size_t surfaceId, RenderStage &renderStage);

        // Core Vulkan objects
        std::unique_ptr<Instance> instance;
        std::unique_ptr<PhysicalDevice> physicalDevice;
        std::unique_ptr<LogicalDevice> logicalDevice;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        // Surfaces and swapchains
        std::vector<std::unique_ptr<Surface>> surfaces;
        std::vector<std::unique_ptr<Swapchain>> swapchains;
        std::vector<std::unique_ptr<PerSurfaceBuffers>> perSurfaceBuffers;

        // Rendering
        std::unique_ptr<Renderer> renderer;
        std::unordered_map<std::string, const Descriptor *> attachments;

        // Command pool management
        std::unordered_map<std::thread::id, std::shared_ptr<CommandPool>> commandPools;

        // Timing for command pool purging
        ElapsedTime elapsedPurge;

        VmaAllocator alloc;

        // additional stuff
        std::unique_ptr<BindlessManager> bindlessMgr;

    public:
        VmaAllocator *GetAllocator()
        {
            return &alloc;
        }

        MulticastDelegate<VkCommandBuffer, std::size_t> &OnRecordViewports() { return onRecordViewports; }
    private:
        MulticastDelegate<VkCommandBuffer, std::size_t> onRecordViewports;
    };

    /**
     * @brief Concepts for Vulkan handles
     */
    template <typename T>
    concept VulkanHandle = requires(T t) {
        { t } -> std::convertible_to<uint64_t>;
    } || std::is_pointer_v<T>;

    /**
     * @brief RAII wrapper for Vulkan handles with custom deleters
     */
    template <VulkanHandle T, auto Deleter>
    class VulkanResource
    {
    public:
        VulkanResource() = default;

        explicit VulkanResource(T handle) noexcept : m_handle(handle) {}

        ~VulkanResource()
        {
            if (m_handle)
                Deleter(m_handle);
        }

        // Delete copy
        VulkanResource(const VulkanResource &) = delete;
        VulkanResource &operator=(const VulkanResource &) = delete;

        // Allow move
        VulkanResource(VulkanResource &&other) noexcept
            : m_handle(std::exchange(other.m_handle, T{}))
        {
        }

        VulkanResource &operator=(VulkanResource &&other) noexcept
        {
            if (this != &other)
            {
                if (m_handle)
                    Deleter(m_handle);
                m_handle = std::exchange(other.m_handle, T{});
            }
            return *this;
        }

        [[nodiscard]] T get() const noexcept
        {
            return m_handle;
        }
        [[nodiscard]] T *ptr() noexcept
        {
            return &m_handle;
        }
        [[nodiscard]] operator T() const noexcept
        {
            return m_handle;
        }
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_handle != T{};
        }

        T release() noexcept
        {
            return std::exchange(m_handle, T{});
        }

        void reset(T newHandle = T{}) noexcept
        {
            if (m_handle)
                Deleter(m_handle);
            m_handle = newHandle;
        }

    private:
        T m_handle{};
    };

    /**
     * @brief Vulkan version helpers
     */
    namespace VulkanVersion
    {
        constexpr uint32_t Make(uint32_t major, uint32_t minor, uint32_t patch = 0) noexcept
        {
            return VK_MAKE_API_VERSION(0, major, minor, patch);
        }

        constexpr uint32_t GetMajor(uint32_t version) noexcept
        {
            return VK_API_VERSION_MAJOR(version);
        }

        constexpr uint32_t GetMinor(uint32_t version) noexcept
        {
            return VK_API_VERSION_MINOR(version);
        }

        constexpr uint32_t GetPatch(uint32_t version) noexcept
        {
            return VK_API_VERSION_PATCH(version);
        }

        constexpr auto Vulkan_1_0 = VK_API_VERSION_1_0;
        constexpr auto Vulkan_1_1 = VK_API_VERSION_1_1;
        constexpr auto Vulkan_1_2 = VK_API_VERSION_1_2;
        constexpr auto Vulkan_1_3 = VK_API_VERSION_1_3;
    }

    /**
     * @brief Extension and feature queries using C++20 ranges
     */
    namespace VulkanFeatures
    {
        /**
         * @brief Check if extensions are supported
         */
        inline bool AreExtensionsSupported(std::span<const char *const> required,
                                           std::span<const VkExtensionProperties> available)
        {
            return std::ranges::all_of(
                required,
                [&](const char *req)
                {
                    return std::ranges::any_of(
                        available, [req](const auto &ext)
                        { return std::string_view(req) == std::string_view(ext.extensionName); });
                });
        }

        /**
         * @brief Get missing extensions
         */
        inline std::vector<std::string_view> GetMissingExtensions(
            std::span<const char *const> required, std::span<const VkExtensionProperties> available)
        {
            std::vector<std::string_view> missing;

            for (const char *req : required)
            {
                if (!std::ranges::any_of(
                        available, [req](const auto &ext)
                        { return std::string_view(req) == std::string_view(ext.extensionName); }))
                {
                    missing.emplace_back(req);
                }
            }

            return missing;
        }
    }

    /**
     * @brief Modern command buffer recording with RAII
     */
    class ScopedCommandBuffer
    {
    public:
        explicit ScopedCommandBuffer(CommandBuffer &cmd, VkCommandBufferUsageFlags flags = 0)
            : m_cmd(cmd)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = flags;
            vkBeginCommandBuffer(m_cmd, &beginInfo);
        }

        ~ScopedCommandBuffer()
        {
            vkEndCommandBuffer(m_cmd);
        }

        // Delete copy and move
        ScopedCommandBuffer(const ScopedCommandBuffer &) = delete;
        ScopedCommandBuffer &operator=(const ScopedCommandBuffer &) = delete;
        ScopedCommandBuffer(ScopedCommandBuffer &&) = delete;
        ScopedCommandBuffer &operator=(ScopedCommandBuffer &&) = delete;

        [[nodiscard]] operator VkCommandBuffer() const noexcept
        {
            return m_cmd;
        }
        [[nodiscard]] VkCommandBuffer get() const noexcept
        {
            return m_cmd;
        }

    private:
        CommandBuffer &m_cmd;
    };

    /**
     * @brief Modern render pass recording with dynamic rendering (Vulkan 1.3+)
     */
    class ScopedDynamicRendering
    {
    public:
        ScopedDynamicRendering(VkCommandBuffer cmd, const VkRenderingInfo &renderingInfo)
            : m_cmd(cmd)
        {
            vkCmdBeginRendering(m_cmd, &renderingInfo);
        }

        ~ScopedDynamicRendering()
        {
            vkCmdEndRendering(m_cmd);
        }

        ScopedDynamicRendering(const ScopedDynamicRendering &) = delete;
        ScopedDynamicRendering &operator=(const ScopedDynamicRendering &) = delete;
        ScopedDynamicRendering(ScopedDynamicRendering &&) = delete;
        ScopedDynamicRendering &operator=(ScopedDynamicRendering &&) = delete;

    private:
        VkCommandBuffer m_cmd;
    };

    /**
     * @brief Modern indirect indexed rendering command recording with RAII
     *
     * Records a single indirect indexed draw command or multiple with count
     */
    class IndirectIndexedRendering : NoCopy, NoMove
    {
    public:
        /**
         * @brief Construct for single indirect draw
         *
         * @param cmd Command buffer to record to
         * @param buffer Buffer containing draw commands
         * @param offset Offset into the buffer
         * @param stride Stride between commands (must be at least sizeof(VkDrawIndexedIndirectCommand))
         */
        IndirectIndexedRendering(VkCommandBuffer cmd,
                                 VkBuffer buffer,
                                 VkDeviceSize offset,
                                 uint32_t stride = sizeof(VkDrawIndexedIndirectCommand))
            : m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_drawCount(1), m_stride(stride)
        {
            vkCmdDrawIndexedIndirect(m_cmd, m_buffer, m_offset, m_drawCount, m_stride);
        }

        /**
         * @brief Construct for multiple indirect draws
         *
         * @param cmd Command buffer to record to
         * @param buffer Buffer containing draw commands
         * @param offset Offset into the buffer
         * @param drawCount Number of draws to execute
         * @param stride Stride between commands
         */
        IndirectIndexedRendering(VkCommandBuffer cmd,
                                 VkBuffer buffer,
                                 VkDeviceSize offset,
                                 uint32_t drawCount,
                                 uint32_t stride)
            : m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_drawCount(drawCount), m_stride(stride)
        {
            vkCmdDrawIndexedIndirect(m_cmd, m_buffer, m_offset, m_drawCount, m_stride);
        };

        /**
         * @brief Get the command buffer
         */
        [[nodiscard]] VkCommandBuffer get() const noexcept
        {
            return m_cmd;
        }

        /**
         * @brief Get the buffer containing draw commands
         */
        [[nodiscard]] VkBuffer getBuffer() const noexcept
        {
            return m_buffer;
        }

        /**
         * @brief Get the number of draws that will be executed
         */
        [[nodiscard]] uint32_t getDrawCount() const noexcept
        {
            return m_drawCount;
        }

    private:
        VkCommandBuffer m_cmd;
        VkBuffer m_buffer;
        VkDeviceSize m_offset;
        uint32_t m_drawCount;
        uint32_t m_stride;
    };

    /**
     * @brief Extended version with indirect count buffer support (Vulkan 1.2+)
     *
     * Uses vkCmdDrawIndexedIndirectCount for when the draw count is on the GPU
     */
    class IndirectIndexedCountRendering : NoCopy, NoMove
    {
    public:
        /**
         * @brief Construct for indirect draws with GPU-controlled count
         *
         * @param cmd Command buffer to record to
         * @param buffer Buffer containing draw commands
         * @param offset Offset into the buffer
         * @param countBuffer Buffer containing the draw count
         * @param countBufferOffset Offset into the count buffer
         * @param maxDrawCount Maximum number of draws to execute
         * @param stride Stride between commands
         */
        IndirectIndexedCountRendering(VkCommandBuffer cmd,
                                      VkBuffer buffer,
                                      VkDeviceSize offset,
                                      VkBuffer countBuffer,
                                      VkDeviceSize countBufferOffset,
                                      uint32_t maxDrawCount,
                                      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand))
            : m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_countBuffer(countBuffer), m_countBufferOffset(countBufferOffset), m_maxDrawCount(maxDrawCount), m_stride(stride)
        {
            vkCmdDrawIndexedIndirectCount(m_cmd,
                                          m_buffer,
                                          m_offset,
                                          m_countBuffer,
                                          m_countBufferOffset,
                                          m_maxDrawCount,
                                          m_stride);
        }

        /**
         * @brief Get the command buffer
         */
        [[nodiscard]] VkCommandBuffer get() const noexcept
        {
            return m_cmd;
        }

        /**
         * @brief Get the buffer containing draw commands
         */
        [[nodiscard]] VkBuffer getBuffer() const noexcept
        {
            return m_buffer;
        }

        /**
         * @brief Get the buffer containing the draw count
         */
        [[nodiscard]] VkBuffer getCountBuffer() const noexcept
        {
            return m_countBuffer;
        }

        /**
         * @brief Get the maximum number of draws that can be executed
         */
        [[nodiscard]] uint32_t getMaxDrawCount() const noexcept
        {
            return m_maxDrawCount;
        }

    private:
        VkCommandBuffer m_cmd;
        VkBuffer m_buffer;
        VkDeviceSize m_offset;
        VkBuffer m_countBuffer;
        VkDeviceSize m_countBufferOffset;
        uint32_t m_maxDrawCount;
        uint32_t m_stride;
    };

    /**
     * @brief Helper function to create an indirect draw command structure
     */
    [[nodiscard]] inline VkDrawIndexedIndirectCommand MakeIndexedIndirectCommand(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance) noexcept
    {
        return VkDrawIndexedIndirectCommand{
            .indexCount = indexCount,
            .instanceCount = instanceCount,
            .firstIndex = firstIndex,
            .vertexOffset = vertexOffset,
            .firstInstance = firstInstance};
    }

    /**
     * @brief Concept to check if a type supports indirect rendering
     */
    template <typename T>
    concept IndirectRenderingCommand = requires(T t, VkCommandBuffer cmd) {
        { t.record(cmd) } -> std::same_as<void>;
    };

    /**
     * @brief Batch multiple indirect rendering commands
     */
    class IndirectBatch
    {
    public:
        explicit IndirectBatch(VkCommandBuffer cmd) : m_cmd(cmd) {}

        template <IndirectRenderingCommand... Commands>
        void record(Commands &&...cmds)
        {
            (cmds.record(m_cmd), ...);
        }

        /**
         * @brief Record a single indirect indexed draw
         */
        void drawIndexedIndirect(VkBuffer buffer,
                                 VkDeviceSize offset,
                                 uint32_t drawCount = 1,
                                 uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) const {
            vkCmdDrawIndexedIndirect(m_cmd, buffer, offset, drawCount, stride);
        }

        /**
         * @brief Record indirect indexed draws with count buffer
         */
        void drawIndexedIndirectCount(VkBuffer buffer,
                                      VkDeviceSize offset,
                                      VkBuffer countBuffer,
                                      VkDeviceSize countBufferOffset,
                                      uint32_t maxDrawCount,
                                      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand))
        {
            vkCmdDrawIndexedIndirectCount(m_cmd,
                                          buffer,
                                          offset,
                                          countBuffer,
                                          countBufferOffset,
                                          maxDrawCount,
                                          stride);
        }

    private:
        VkCommandBuffer m_cmd;
    };

} // namespace SF::Engine