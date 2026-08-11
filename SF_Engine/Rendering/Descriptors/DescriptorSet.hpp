#pragma once

#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Pipelines/Pipeline.hpp>

namespace SF::Engine
{
    class DescriptorSet
    {
    public:
        explicit DescriptorSet(const Pipeline &pipeline);
        ~DescriptorSet();

        // Non-copyable, non-movable : owns a VkDescriptorSet handle.
        DescriptorSet(const DescriptorSet &) = delete;
        DescriptorSet &operator=(const DescriptorSet &) = delete;
        DescriptorSet(DescriptorSet &&) = delete;
        DescriptorSet &operator=(DescriptorSet &&) = delete;

        static void Update(const std::vector<VkWriteDescriptorSet> &descriptorWrites);

        void BindDescriptor(const CommandBuffer &commandBuffer) const;

        const VkDescriptorSet &GetDescriptorSet() const { return descriptorSet; }

    private:
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint{};
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
}
