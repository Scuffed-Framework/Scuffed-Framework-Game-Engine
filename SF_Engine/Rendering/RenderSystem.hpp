#pragma once

#include <Engine/Module.hpp>
#include <Platform/Windowing/Surface.hpp>
#include <Platform/Windowing/WindowManager.hpp>
#include "RHI/Adapter/Instance.hpp"
#include "RHI/Adapter/LogicalDevice.hpp"
#include "RHI/Adapter/PhysicalDevice.hpp"
#include "RHI/Bindless/Bindless.hpp"
#include "RHI/Commands/CommandBuffer.hpp"
#include "RHI/Commands/CommandPool.hpp"
#include "RHI/Renderpass/RhiSwapchain.hpp"
#include "Renderer.hpp"

#include <Communication/Delegates/MultiCastDelegate.hpp>
#include <UtilityClasses/NoCopy.hpp>

#include <filesystem>
#include <span>
#include <thread>

namespace SF::Engine
{
    using namespace std;

    class BindlessManager;
    class SMARTRenderSystemConfig;

    /**
     * @brief Module that manages the Vulkan instance, devices, surfaces, and rendering
     * infrastructure.
     */
    class RenderSystem final : public ModuleRegistrar<RenderSystem>
    {
        friend class ModuleRegistrar<RenderSystem>;
        REGISTER_MODULE(RenderSystem, Module::Stage::Render, Module::Requires<WindowManager>{});

    public:
        RenderSystem();
        ~RenderSystem() override;

        void PostInit();
        void PreShutdown() const;

        // Module interface implementation
        void Update() override;

        Module::Stage GetStage() const override { return Module::Stage::Render; }
        TypeId GetTypeId() const override { return TypeInfo<Module>::GetTypeId<RenderSystem>(); }
        string_view GetName() const override { return "RenderSystem"; }

        /**
         * @brief Convert Vulkan result to string (for debugging)
         */
        static string StrVkResult(VkResult result);

        /**
         * @brief Check Vulkan result and throw on error
         */
        static void CheckVkResult(VkResult result);

        /**
         * @brief Takes a screenshot of the current swapchain image
         */
        void CaptureScreenshot(const filesystem::path &filename, size_t surfaceId = 0) const;

        /**
         * @brief Get or create command pool for current thread
         */
        const shared_ptr<CommandPool> &GetCommandPool(const thread::id &threadId = this_thread::get_id());

        /**
         * @brief Get render stage by index
         */
        const RhiRenderStage *GetRenderStage(uint32_t index) const;

        /**
         * @brief Get attachment descriptor by name
         */
        const Descriptor *GetAttachment(const string &name) const;

        /**
         * @brief Incremented every time render-stage attachments are rebuilt. Passes that cache
         * descriptors pointing at attachment images must compare THIS, not attachment pointers:
         * freed Image2d objects' addresses are routinely reused by the next allocation, so a
         * pointer compare can falsely report "unchanged" and leave a descriptor on a destroyed view.
         */
        uint64_t GetAttachmentGeneration() const noexcept { return attachmentGeneration; }

        // Device and resource accessors
        const Instance *GetInstance() const noexcept { return instance.get(); }
        const PhysicalDevice *GetPhysicalDevice() const noexcept { return physicalDevice.get(); }
        const LogicalDevice *GetLogicalDevice() const noexcept { return logicalDevice.get(); }
        VkPipelineCache GetPipelineCache() const noexcept { return pipelineCache; }

        /**
         * @brief Get surface by index
         */
        const Surface *GetSurface(size_t id) const noexcept
        {
            return id < surfaces.size() ? surfaces[id].get() : nullptr;
        }

        /**
         * @brief Get swapchain by index
         */
        const RhiSwapchain *GetSwapchain(size_t id) const noexcept
        {
            return id < swapchains.size() ? swapchains[id].get() : nullptr;
        }

        void SetFramebufferResized(size_t id) const
        {
            if (id < perSurfaceBuffers.size() && perSurfaceBuffers[id])
                perSurfaceBuffers[id]->framebufferResized = true;
        }

        /**
         * @brief Get number of surfaces
         */
        size_t GetSurfaceCount() const noexcept { return surfaces.size(); }

        void SetRenderer(unique_ptr<Renderer> &&r) { renderer = std::move(r); }

        Renderer *GetRenderer() const noexcept { return renderer.get(); }

        BindlessManager *GetBindlessManager() const { return bindlessMgr.get(); }

        /**
         * @brief Rebuild render stages, swapchain, and framebuffers.
         * Safe to call from Stage::Normal (outside the render loop).
         */
        void ResetRenderStages();

        static uint32_t GetVkAPIVersion();

    private:
        /**
         * @brief Per-surface synchronization and command buffers
         */
        struct PerSurfaceBuffers
        {
            vector<VkSemaphore> presentCompletes;
            vector<VkSemaphore> renderCompletes;
            vector<VkFence> flightFences;
            vector<unique_ptr<CommandBuffer>> commandBuffers;

            size_t currentFrame     = 0;
            bool framebufferResized = false;
        };

        // Helper to enumerate with index
        template<typename Container>
        static auto Enumerate(Container &container)
        {
            struct Iterator
            {
                size_t index;
                Container::iterator iter;

                auto operator*() { return make_pair(index, ref(*iter)); }
                Iterator &operator++()
                {
                    ++index;
                    ++iter;
                    return *this;
                }
                bool operator!=(const Iterator &other) const { return iter != other.iter; }
            };

            struct EnumerateWrapper
            {
                Container &cont;
                auto begin() { return Iterator{0, cont.begin()}; }
                auto end() { return Iterator{0, cont.end()}; }
            };

            return EnumerateWrapper{container};
        }

        // Initialization helpers
        void CreatePipelineCache();

        // Render loop helpers
        void RecreateSwapchain();
        void RecreateCommandBuffers(size_t surfaceId) const;
        void RecreatePass();
        void RecreateAttachmentsMap();

        bool StartRenderpass(size_t surfaceId, const RhiRenderStage &renderStage) const;
        void EndRenderpass(size_t surfaceId, const RhiRenderStage &renderStage) const;

        // Core Vulkan objects
        unique_ptr<Instance> instance;
        unique_ptr<PhysicalDevice> physicalDevice;
        unique_ptr<LogicalDevice> logicalDevice;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        // Surfaces and swapchains
        vector<unique_ptr<Surface>> surfaces;
        vector<unique_ptr<RhiSwapchain>> swapchains;
        vector<unique_ptr<PerSurfaceBuffers>> perSurfaceBuffers;

        // Rendering
        unique_ptr<Renderer> renderer;
        unordered_map<string, const Descriptor *> attachments;
        uint64_t attachmentGeneration = 1;

        // Command pool management
        unordered_map<thread::id, shared_ptr<CommandPool>> commandPools;

        // Timing for command pool purging
        ElapsedTime elapsedPurge;

        VmaAllocator alloc = VK_NULL_HANDLE;

        unique_ptr<BindlessManager> bindlessMgr;
        unique_ptr<SMARTRenderSystemConfig> config;

    public:
        VmaAllocator *GetAllocator() { return &alloc; }

        MulticastDelegate<VkCommandBuffer, size_t> &OnRecordViewports() { return onRecordViewports; }

    private:
        MulticastDelegate<VkCommandBuffer, size_t> onRecordViewports;
    };

    /**
     * @brief Concepts for Vulkan handles
     */
    template<typename T>
    concept VulkanHandle = requires(T t) {
        { t } -> convertible_to<uint64_t>;
    } || is_pointer_v<T>;

    /**
     * @brief RAII wrapper for Vulkan handles with custom deleters
     */
    template<VulkanHandle T, auto Deleter>
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
        VulkanResource(const VulkanResource &)            = delete;
        VulkanResource &operator=(const VulkanResource &) = delete;

        // Allow move
        VulkanResource(VulkanResource &&other) noexcept : m_handle(exchange(other.m_handle, T{})) {}

        VulkanResource &operator=(VulkanResource &&other) noexcept
        {
            if (this != &other)
            {
                if (m_handle)
                    Deleter(m_handle);
                m_handle = exchange(other.m_handle, T{});
            }
            return *this;
        }

        [[nodiscard]] T get() const noexcept { return m_handle; }
        [[nodiscard]] T *ptr() noexcept { return &m_handle; }
        [[nodiscard]] operator T() const noexcept { return m_handle; }
        [[nodiscard]] explicit operator bool() const noexcept { return m_handle != T{}; }

        T release() noexcept { return exchange(m_handle, T{}); }

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

        constexpr uint32_t GetMajor(uint32_t version) noexcept { return VK_API_VERSION_MAJOR(version); }

        constexpr uint32_t GetMinor(uint32_t version) noexcept { return VK_API_VERSION_MINOR(version); }

        constexpr uint32_t GetPatch(uint32_t version) noexcept { return VK_API_VERSION_PATCH(version); }

        constexpr auto Vulkan_1_0 = VK_API_VERSION_1_0;
        constexpr auto Vulkan_1_1 = VK_API_VERSION_1_1;
        constexpr auto Vulkan_1_2 = VK_API_VERSION_1_2;
        constexpr auto Vulkan_1_3 = VK_API_VERSION_1_3;
    } // namespace VulkanVersion

    /**
     * @brief Extension and feature queries using C++20 ranges
     */
    namespace VulkanFeatures
    {
        /**
         * @brief Check whether all specified extensions are supported.
         * @param required Braced list of extension names.
         * @return True if every required extension is supported.
         */
        inline bool AreExtensionsSupported(initializer_list<string_view> required)
        {
            return ranges::all_of(required,
                                  [&](string_view req)
                                  {
                                      return ranges::any_of(RenderSystem::Get()->GetPhysicalDevice()->GetExtensions(),
                                                            [&](const VkExtensionProperties &ext)
                                                            { return req == ext.extensionName; });
                                  });
        }

        /**
         * @brief Get missing extensions.
         * @param required Braced list of extension names.
         * @return Names of extensions not supported by the physical device.
         */
        inline vector<string_view> GetMissingExtensions(initializer_list<string_view> required)
        {
            vector<string_view> missing;

            for (string_view req: required)
            {
                if (!ranges::any_of(RenderSystem::Get()->GetPhysicalDevice()->GetExtensions(),
                                    [&](const VkExtensionProperties &ext) { return req == ext.extensionName; }))
                {
                    missing.emplace_back(req);
                }
            }

            return missing;
        }

        /**
         * @brief Check whether a single extension is supported.
         */
        inline bool IsExtensionSupported(string_view name)
        {
            return RenderSystem::Get()->GetPhysicalDevice()->IsExtensionSupported(name);
        }

    } // namespace VulkanFeatures

    // Scalable Multi-threaded Adaptive Rendering Technology
    class SMARTRenderSystemConfig
    {
    public:
        bool VulkanValidationLayersAllowed = false;
        struct EnabledExtensions
        {
            // Descriptor management
            bool descriptor_heap     = VulkanFeatures::IsExtensionSupported("VK_EXT_descriptor_heap");
            bool descriptor_buffer   = VulkanFeatures::IsExtensionSupported(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
            bool descriptor_indexing = VulkanFeatures::IsExtensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

            // Shader and pipeline functionality
            bool shader_object = VulkanFeatures::IsExtensionSupported(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
            bool extended_dynamic_state =
                    VulkanFeatures::IsExtensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
            bool extended_dynamic_state2 =
                    VulkanFeatures::IsExtensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
            bool extended_dynamic_state3 =
                    VulkanFeatures::IsExtensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

            // Rendering
            bool dynamic_rendering = VulkanFeatures::IsExtensionSupported("VK_KHR_dynamic_rendering");
            bool synchronization2  = VulkanFeatures::IsExtensionSupported("VK_KHR_synchronization2");
            bool maintenance4      = VulkanFeatures::IsExtensionSupported("VK_KHR_maintenance4");
            bool maintenance5      = VulkanFeatures::IsExtensionSupported("VK_KHR_maintenance5");
            bool maintenance6      = VulkanFeatures::IsExtensionSupported("VK_KHR_maintenance6");

            // GPU-driven rendering
            bool mesh_shader = VulkanFeatures::IsExtensionSupported("VK_EXT_mesh_shader");
            bool device_generated_commands =
                    VulkanFeatures::IsExtensionSupported(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);

            // Ray tracing
            bool acceleration_structure =
                    VulkanFeatures::IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            bool ray_tracing_pipeline =
                    VulkanFeatures::IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            bool ray_query = VulkanFeatures::IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            bool deferred_host_operations =
                    VulkanFeatures::IsExtensionSupported(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

            // Memory and synchronization
            bool buffer_device_address =
                    VulkanFeatures::IsExtensionSupported(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            bool memory_budget      = VulkanFeatures::IsExtensionSupported(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
            bool memory_priority    = VulkanFeatures::IsExtensionSupported(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
            bool timeline_semaphore = VulkanFeatures::IsExtensionSupported(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

            // Diagnostics
            bool debug_utils = VulkanFeatures::IsExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        };

        struct EnabledFeatures
        {
            // Descriptor management
            bool DescriptorHeap                                = true;
            bool DescriptorHeapCaptureReplay                   = false;
            bool RuntimeDescriptorArray                        = true;
            bool DescriptorBindingPartiallyBound               = true;
            bool DescriptorBindingVariableDescriptorCount      = true;
            bool DescriptorBindingSampledImageUpdateAfterBind  = true;
            bool DescriptorBindingStorageBufferUpdateAfterBind = true;

            // Shader capabilities
            bool ShaderInt64              = true;
            bool ShaderFloat64            = false;
            bool ShaderInt16              = true;
            bool ShaderFloat16            = true;
            bool ShaderBufferInt64Atomics = false;
            bool ShaderSharedInt64Atomics = false;

            // Rendering
            bool DynamicRendering  = true;
            bool Synchronization2  = true;
            bool FillModeNonSolid  = true;
            bool WideLines         = false;
            bool SamplerAnisotropy = true;
            bool SampleRateShading = true;

            // GPU-driven rendering
            bool MultiDrawIndirect         = true;
            bool DrawIndirectFirstInstance = true;
            bool ShaderDrawParameters      = true;
            bool MeshShader                = true;
            bool TaskShader                = true;

            // Ray tracing
            bool AccelerationStructure = true;
            bool RayTracingPipeline    = true;
            bool RayQuery              = true;
            bool BufferDeviceAddress   = true;

            // Performance
            bool TimelineSemaphore       = true;
            bool Synchronization2Enabled = true;
            bool MemoryPriority          = true;
        };
    };

    /**
     * @brief Modern command buffer recording with RAII
     */
    class ScopedCommandBuffer
    {
    public:
        explicit ScopedCommandBuffer(CommandBuffer &cmd, VkCommandBufferUsageFlags flags = 0) : m_cmd(cmd)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags                    = flags;
            vkBeginCommandBuffer(m_cmd, &beginInfo);
        }

        ~ScopedCommandBuffer() { vkEndCommandBuffer(m_cmd); }

        // Delete copy and move
        ScopedCommandBuffer(const ScopedCommandBuffer &)            = delete;
        ScopedCommandBuffer &operator=(const ScopedCommandBuffer &) = delete;
        ScopedCommandBuffer(ScopedCommandBuffer &&)                 = delete;
        ScopedCommandBuffer &operator=(ScopedCommandBuffer &&)      = delete;

        [[nodiscard]] operator VkCommandBuffer() const noexcept { return m_cmd; }
        [[nodiscard]] VkCommandBuffer get() const noexcept { return m_cmd; }

    private:
        CommandBuffer &m_cmd;
    };

    /**
     * @brief Modern render pass recording with dynamic rendering (Vulkan 1.3+)
     */
    class ScopedDynamicRendering
    {
    public:
        ScopedDynamicRendering(VkCommandBuffer cmd, const VkRenderingInfo &renderingInfo) : m_cmd(cmd)
        {
            vkCmdBeginRendering(m_cmd, &renderingInfo);
        }

        ~ScopedDynamicRendering() { vkCmdEndRendering(m_cmd); }

        ScopedDynamicRendering(const ScopedDynamicRendering &)            = delete;
        ScopedDynamicRendering &operator=(const ScopedDynamicRendering &) = delete;
        ScopedDynamicRendering(ScopedDynamicRendering &&)                 = delete;
        ScopedDynamicRendering &operator=(ScopedDynamicRendering &&)      = delete;

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
        IndirectIndexedRendering(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset,
                                 uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) :
            m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_drawCount(1), m_stride(stride)
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
        IndirectIndexedRendering(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                                 uint32_t stride) :
            m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_drawCount(drawCount), m_stride(stride)
        {
            vkCmdDrawIndexedIndirect(m_cmd, m_buffer, m_offset, m_drawCount, m_stride);
        };

        /**
         * @brief Get the command buffer
         */
        [[nodiscard]] VkCommandBuffer Get() const noexcept { return m_cmd; }

        /**
         * @brief Get the buffer containing draw commands
         */
        [[nodiscard]] VkBuffer GetBuffer() const noexcept { return m_buffer; }

        /**
         * @brief Get the number of draws that will be executed
         */
        [[nodiscard]] uint32_t GetDrawCount() const noexcept { return m_drawCount; }

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
        IndirectIndexedCountRendering(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                      VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) :
            m_cmd(cmd), m_buffer(buffer), m_offset(offset), m_countBuffer(countBuffer),
            m_countBufferOffset(countBufferOffset), m_maxDrawCount(maxDrawCount), m_stride(stride)
        {
            vkCmdDrawIndexedIndirectCount(m_cmd, m_buffer, m_offset, m_countBuffer, m_countBufferOffset, m_maxDrawCount,
                                          m_stride);
        }

        /**
         * @brief Get the command buffer
         */
        [[nodiscard]] VkCommandBuffer Get() const noexcept { return m_cmd; }

        /**
         * @brief Get the buffer containing draw commands
         */
        [[nodiscard]] VkBuffer GetBuffer() const noexcept { return m_buffer; }

        /**
         * @brief Get the buffer containing the draw count
         */
        [[nodiscard]] VkBuffer GetCountBuffer() const noexcept { return m_countBuffer; }

        /**
         * @brief Get the maximum number of draws that can be executed
         */
        [[nodiscard]] uint32_t GetMaxDrawCount() const noexcept { return m_maxDrawCount; }

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
    [[nodiscard]] inline VkDrawIndexedIndirectCommand
    MakeIndexedIndirectCommand(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                               uint32_t firstInstance) noexcept
    {
        return VkDrawIndexedIndirectCommand{.indexCount    = indexCount,
                                            .instanceCount = instanceCount,
                                            .firstIndex    = firstIndex,
                                            .vertexOffset  = vertexOffset,
                                            .firstInstance = firstInstance};
    }

    /**
     * @brief Concept to check if a type supports indirect rendering
     */
    template<typename T>
    concept IndirectRenderingCommand = requires(T t, VkCommandBuffer cmd) {
        { t.record(cmd) } -> same_as<void>;
    };

    /**
     * @brief Batch multiple indirect rendering commands
     */
    class IndirectBatch
    {
    public:
        explicit IndirectBatch(VkCommandBuffer cmd) : m_cmd(cmd) {}

        template<IndirectRenderingCommand... Commands>
        void Record(Commands &&...cmds)
        {
            (cmds.record(m_cmd), ...);
        }

        /**
         * @brief Record a single indirect indexed draw
         */
        void DrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount = 1,
                                 uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) const
        {
            vkCmdDrawIndexedIndirect(m_cmd, buffer, offset, drawCount, stride);
        }

        /**
         * @brief Record indirect indexed draws with count buffer
         */
        void DrawIndexedIndirectCount(VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                      VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) const
        {
            vkCmdDrawIndexedIndirectCount(m_cmd, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
        }

    private:
        VkCommandBuffer m_cmd;
    };

} // namespace SF::Engine
