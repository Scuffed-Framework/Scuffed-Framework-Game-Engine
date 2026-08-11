#pragma once

#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include "LightManager.hpp"
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Two-phase compute PipelinePass: cluster AABB build + light culling.
     *
     * Both phases run in PreRender() : which executes OUTSIDE the renderpass
     * (the engine calls PreRender before vkCmdBeginRenderPass). This is the only
     * correct way to dispatch compute commands in the current pipeline.
     *
     * Phase 1 : ClusterBuild (once on first frame and on resize):
     *   CLUSTER_X × CLUSTER_Y × CLUSTER_Z threads, one per cluster.
     *   Writes view-space AABBs to clusterSSBO.
     *
     * Phase 2 : ClusterCull (every frame):
     *   64-thread groups × ceil(CLUSTER_COUNT/64) dispatches.
     *   Tests each light sphere against each cluster AABB.
     *   Writes light indices + per-cluster headers into lightIndexSSBO / lightListSSBO.
     *
     * A compute→compute memory barrier separates phases 1 and 2, and a final
     * compute→fragment barrier ensures light lists are visible to the lighting pass.
     */
    class ClusterCullPipelinePass : public PipelinePass
    {
    public:
        explicit ClusterCullPipelinePass(Pipeline::Stage stage, LightManager &lightManager);
        ~ClusterCullPipelinePass() override = default;

        /// Called before the renderpass : safe for compute dispatches.
        void PreRender(const CommandBuffer &commandBuffer) override;

        /// No-op: all work is done in PreRender.
        void Render(const CommandBuffer & /*commandBuffer*/) override {}

        /// Mark clusters as dirty (call after swapchain resize).
        void MarkDirty() { clustersDirty_ = true; }

    private:
        void DispatchBuild(const CommandBuffer &commandBuffer);
        void DispatchCull(const CommandBuffer &commandBuffer);
        void InsertComputeToComputeBarrier(const CommandBuffer &commandBuffer) const;
        void InsertComputeToFragmentBarrier(const CommandBuffer &commandBuffer) const;

        LightManager &lm_;

        std::unique_ptr<ComputePipeline> buildPipeline_;
        std::unique_ptr<ComputePipeline> cullPipeline_;
        std::unique_ptr<DescriptorSet> buildDescSet_;
        std::unique_ptr<DescriptorSet> cullDescSet_;

        bool clustersDirty_ = true;
    };
}
