#pragma once
#include <UtilityClasses/NoCopy.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Math/KVP.hpp>
#ifdef Bool
#undef Bool
#endif
#include <slang.h>
#include <slang-com-ptr.h>

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
        uint32 set;
        uint32 binding;
    };
    
    using BindlessIndex = KeyValuePair<uint32, uint32>;
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
        void FreeUAV(BindlessIndex &index, Image fallback);

        // Free ssbo bindless.
        void FreeStorageBuffer(BindlessIndex &index, std::shared_ptr<Buffer> fallback);
        void FreeUniformBuffer(BindlessIndex &index, std::shared_ptr<Buffer> fallback);

        void VerifyShaderLayout(const std::vector<BindlessReflectionData> &reflectionData);

        const VkDescriptorSetLayout &getSetLayout() const { return m_setLayout; }
        const VkDescriptorSet &getSet() const { return m_set; }

        void Bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const
        {
            vkCmdBindDescriptorSets(cmd, bindPoint, layout, 0, 1, &m_set, 0, nullptr);
        }

    private:
        uint32 RequireIndex(EBindingType type);
        void FreeIndex(EBindingType type, uint32 index);

    private:
        static constexpr auto kBindingCount = static_cast<uint32>(EBindingType::MAX);

        VkDescriptorPool m_pool = VK_NULL_HANDLE;
        VkDescriptorSet m_set = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;

        struct BindingConfig
        {
            VkDescriptorType type;
            uint32 count;
            uint32 limit;
        };
        BindingConfig m_bindingConfigs[kBindingCount];

        std::mutex m_lockCount;
        std::queue<uint32> m_freeCount[kBindingCount];
        uint32 m_usedCount[kBindingCount];
    };

    std::vector<SF::Engine::BindlessReflectionData> ReflectBindlessLayout(Slang::ComPtr<slang::IComponentType> linkedProgram)
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
            data.name = param->getName();
            data.binding = param->getBindingIndex();
            data.set = param->getBindingSpace();

            reflectedBindings.push_back(data);
        }
        return reflectedBindings;
    }
}