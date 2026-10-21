#pragma once
#include <Math/KVP.hpp>
#include <Rendering/RenderSystem.hpp>
#include <UtilityClasses/NoCopy.hpp>
#ifdef Bool
    #undef Bool
#endif
#include <slang-com-ptr.h>
#include <slang.h>
namespace SF::Engine
{
    enum class EBindingType
    {
        BindlessStorageBuffer = 0,
        BindlessUniformBuffer,
        BindlessSampledImage,
        BindlessStorageImage,
        BindlessSampler,
        BindlessUniformTexelBuffer,
        BindlessStorageTexelBuffer,
        MAX
    };

    struct BindlessReflectionData
    {
        std::string name;
        uint32_t set;
        uint32_t binding;
    };

    using BindlessIndex = KeyValuePair<uint32_t, uint32_t>;

    class BindlessManager : NoCopy
    {
    public:
        explicit BindlessManager();
        ~BindlessManager();
        [[nodiscard]] BindlessIndex RegisterSampler(VkSampler sampler);
        [[nodiscard]] BindlessIndex RegisterSRV(VkImageView view);
        [[nodiscard]] BindlessIndex RegisterUAV(VkImageView view);
        [[nodiscard]] BindlessIndex RegisterStorageBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
        [[nodiscard]] BindlessIndex RegisterUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
        void FreeSRV(BindlessIndex &index, Image fallback);
        void FreeSRV(BindlessIndex &index);
        void FreeUAV(BindlessIndex &index, Image fallback);
        void FreeStorageBuffer(BindlessIndex &index, const std::shared_ptr<Buffer> &fallback);
        void FreeUniformBuffer(BindlessIndex &index, const std::shared_ptr<Buffer> &fallback);
        void VerifyShaderLayout(const std::vector<BindlessReflectionData> &reflectionData);
        const VkDescriptorSetLayout &getSetLayout() const { return m_setLayout; }
        const VkDescriptorSet &getSet() const { return m_set; }
        void Bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const
        {
            vkCmdBindDescriptorSets(cmd, bindPoint, layout, 0, 1, &m_set, 0, nullptr);
        }

        void Tick();

        void FlushAllPendingFrees();

    private:
        uint32_t RequireIndex(EBindingType type);
        void FreeIndexImmediate(EBindingType type, uint32_t index);

        static constexpr uint32_t kFramesInFlight = 3;

        struct PendingFree
        {
            EBindingType type;
            uint32_t index;
            uint64_t freedOnFrame;
        };

    private:
        static constexpr auto kBindingCount = static_cast<uint32_t>(EBindingType::MAX);
        VkDescriptorPool m_pool             = VK_NULL_HANDLE;
        VkDescriptorSet m_set               = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_setLayout   = VK_NULL_HANDLE;
        struct BindingConfig
        {
            VkDescriptorType type;
            uint32_t count;
            uint32_t limit;
        };
        BindingConfig m_bindingConfigs[kBindingCount];
        std::mutex m_lockCount;
        std::queue<uint32_t> m_freeCount[kBindingCount];
        uint32_t m_usedCount[kBindingCount];

        std::vector<PendingFree> m_pendingFrees;
        uint64_t m_currentFrame = 0;
    };

    inline std::vector<SF::Engine::BindlessReflectionData>
    ReflectBindlessLayout(Slang::ComPtr<slang::IComponentType> linkedProgram)
    {
        std::vector<SF::Engine::BindlessReflectionData> reflectedBindings;
        slang::ProgramLayout *layout = linkedProgram->getLayout();
        if (!layout)
            return reflectedBindings;
        unsigned paramCount = layout->getParameterCount();
        for (unsigned i = 0; i < paramCount; ++i)
        {
            slang::VariableLayoutReflection *param = layout->getParameterByIndex(i);
            SF::Engine::BindlessReflectionData data;
            data.name    = param->getName();
            data.binding = param->getBindingIndex();
            data.set     = param->getBindingSpace();
            reflectedBindings.push_back(data);
        }
        return reflectedBindings;
    }
} // namespace SF::Engine
