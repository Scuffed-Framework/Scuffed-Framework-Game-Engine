#include "DescriptorSet.hpp"

#include "Rendering/RenderSystem.hpp"

namespace SF::Engine
{
    DescriptorSet::DescriptorSet(const Pipeline &pipeline)
        : pipelineLayout(pipeline.GetPipelineLayout()), pipelineBindPoint(pipeline.GetPipelineBindPoint()), descriptorPool(pipeline.GetDescriptorPool())
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        VkDescriptorSetLayout layouts[1] = {pipeline.GetDescriptorSetLayout()};

        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = descriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = layouts;
        RenderSystem::CheckVkResult(vkAllocateDescriptorSets(*logicalDevice, &info, &descriptorSet));
    }

    DescriptorSet::~DescriptorSet()
    {
        // Do NOT call vkFreeDescriptorSets here.
        //
        // Descriptor sets are implicitly freed when their pool is destroyed.
        // The pool is owned by the pipeline (RenderPipeline / ComputePipeline)
        // and destroyed in its destructor via the stored device_ handle.
        //
        // Calling vkFreeDescriptorSets here would cause two problems:
        //   1. During shutdown: RenderSystem::Get() is dead : dangling pointer crash.
        //   2. At runtime: the validation layer wraps handles (see dispatch_object.cpp
        //      UpdateDescriptorSets / Unwrap). Freeing a set removes it from the
        //      layer's tracking table. If the handle value gets reused by the next
        //      allocation, writes to the new set fail with "Invalid VkDescriptorSet"
        //      because the layer sees it as already-freed.
    }

    void DescriptorSet::Update(const std::vector<VkWriteDescriptorSet> &descriptorWrites)
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        vkUpdateDescriptorSets(*logicalDevice,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }

    void DescriptorSet::BindDescriptor(const CommandBuffer &commandBuffer) const
    {
        vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    }
}
