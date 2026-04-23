#include "ClusterCullPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include "LightingTypes.hpp"
#include <crtdbg.h>

namespace SF::Engine
{
    static VkWriteDescriptorSet CullUBO(VkDescriptorSet d, uint32_t b,
                                        const VkDescriptorBufferInfo *i)
    {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d;
        w.dstBinding = b;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = i;
        return w;
    }
    static VkWriteDescriptorSet CullSSBO(VkDescriptorSet d, uint32_t b,
                                         const VkDescriptorBufferInfo *i)
    {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d;
        w.dstBinding = b;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = i;
        return w;
    }

    ClusterCullPipelinePass::ClusterCullPipelinePass(Pipeline::Stage stage, LightManager &lm)
        : PipelinePass(stage), lm_(lm)
    {
        _ASSERTE(_CrtCheckMemory()); // HEAP CHECK A
        buildPipeline_ = std::make_unique<ComputePipeline>("Shaders/Lighting/ClusterBuild.shader");
        _ASSERTE(_CrtCheckMemory()); // HEAP CHECK B (after ClusterBuild pipeline)
        cullPipeline_ = std::make_unique<ComputePipeline>("Shaders/Lighting/ClusterCull.shader");
        _ASSERTE(_CrtCheckMemory()); // HEAP CHECK C (after ClusterCull pipeline)

        buildDescSet_ = std::make_unique<DescriptorSet>(*buildPipeline_);
        _ASSERTE(_CrtCheckMemory()); // HEAP CHECK D (after buildDescSet)
        cullDescSet_ = std::make_unique<DescriptorSet>(*cullPipeline_);
        _ASSERTE(_CrtCheckMemory()); // HEAP CHECK E (after cullDescSet)

        // ClusterBuild: bind=0 frame UBO, bind=1 cluster SSBO
        {
            VkDescriptorBufferInfo fi{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            VkDescriptorBufferInfo ci{lm_.GetClusterSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            DescriptorSet::Update({CullUBO(buildDescSet_->GetDescriptorSet(), 0, &fi),
                                   CullSSBO(buildDescSet_->GetDescriptorSet(), 1, &ci)});
        }

        // ClusterCull: bind=0 frame, bind=1 lights, bind=2 clusters,
        //              bind=3 lightLists, bind=4 indices
        {
            VkDescriptorBufferInfo fi{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            VkDescriptorBufferInfo li{lm_.GetLightSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            VkDescriptorBufferInfo ci{lm_.GetClusterSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            VkDescriptorBufferInfo ls{lm_.GetLightListSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            VkDescriptorBufferInfo ix{lm_.GetLightIndexSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
            DescriptorSet::Update({
                CullUBO(cullDescSet_->GetDescriptorSet(), 0, &fi),
                CullSSBO(cullDescSet_->GetDescriptorSet(), 1, &li),
                CullSSBO(cullDescSet_->GetDescriptorSet(), 2, &ci),
                CullSSBO(cullDescSet_->GetDescriptorSet(), 3, &ls),
                CullSSBO(cullDescSet_->GetDescriptorSet(), 4, &ix),
            });
        }
    }

    void ClusterCullPipelinePass::InsertComputeToComputeBarrier(
        const CommandBuffer &commandBuffer) const
    {
        VkMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &b, 0, nullptr, 0, nullptr);
    }

    void ClusterCullPipelinePass::InsertComputeToFragmentBarrier(
        const CommandBuffer &commandBuffer) const
    {
        // Ensure light index + list SSBOs are visible to the fragment shader
        VkMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 1, &b, 0, nullptr, 0, nullptr);
    }

    void ClusterCullPipelinePass::DispatchBuild(const CommandBuffer &commandBuffer)
    {
        buildPipeline_->BindPipeline(commandBuffer);
        buildDescSet_->BindDescriptor(commandBuffer);
        vkCmdDispatch(commandBuffer,
                      Lighting::CLUSTER_X, Lighting::CLUSTER_Y, Lighting::CLUSTER_Z);
    }

    void ClusterCullPipelinePass::DispatchCull(const CommandBuffer &commandBuffer)
    {
        cullPipeline_->BindPipeline(commandBuffer);
        cullDescSet_->BindDescriptor(commandBuffer);
        constexpr uint32_t LOCAL = 64;
        const uint32_t groups = (Lighting::CLUSTER_COUNT + LOCAL - 1) / LOCAL;
        vkCmdDispatch(commandBuffer, groups, 1, 1);
    }

    void ClusterCullPipelinePass::PreRender(const CommandBuffer &commandBuffer)
    {
        // Phase 1: build cluster AABBs (only when dirty : first frame or resize)
        if (clustersDirty_)
        {
            DispatchBuild(commandBuffer);
            InsertComputeToComputeBarrier(commandBuffer);
            clustersDirty_ = false;
        }

        // Phase 2: cull lights into clusters (every frame)
        DispatchCull(commandBuffer);

        // Barrier: compute writes → fragment reads
        InsertComputeToFragmentBarrier(commandBuffer);
    }
}
