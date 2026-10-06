#pragma once

#include <memory>
#include <vector>

#include <Math/Vectors/Vector.hpp>
#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <Rendering/RHI/Pipelines/ComputePipeline.hpp>

namespace SF::Engine
{
    /**
     * @brief Builds a hierarchical-Z (max-reduction) depth pyramid from the "gbuf_depth"
     * attachment every frame, for a march (SSR's Trace.shader) to walk with adaptive step sizes
     * instead of fixed screen-space steps.
     *
     * Reversed-Z: near = 1, far -> 0, so max-reduction keeps the CLOSEST depth of each cell.
     *
     * Pure compute, stage-less like ClusterCullPipelinePass: all real work happens in
     * PreRender(); Render() is a no-op. Must be registered at a stage whose PreRender runs AFTER
     * the gbuffer renderpass has finished (so gbuf_depth is in DEPTH_STENCIL_READ_ONLY_OPTIMAL)
     * and BEFORE whatever consumes GetPyramid() -- SceneRenderer registers it at Stage{1, 0},
     * ahead of SSR at Stage{1, 1}.
     *
     * Level indexing offset: this pass's pyramid mip 0 IS "Hi-Z level 1" (half resolution).
     * "Hi-Z level 0" is the real depth buffer, sampled directly -- no wasted full-res copy.
     * A consumer wanting level L: L == 0 -> sample gbuf_depth, L >= 1 -> sample GetPyramid()
     * at mip (L - 1). Level L has resolution max(1, depthRes >> L).
     *
     * The pyramid image lives permanently in VK_IMAGE_LAYOUT_GENERAL (storage write + sampled
     * read, no layout transitions); consumers bind it as a sampled image in GENERAL.
     *
     * One dispatch + barrier per mip level, not a single SPD-style pass: simpler, and the cost is
     * a handful of tiny dispatches.
     */
    class HiZPipelinePass : public EngineRenderpass
    {
    public:
        explicit HiZPipelinePass(Pipeline::Stage stage);
        ~HiZPipelinePass() override;

        void PreRender(const CommandBuffer &commandBuffer) override;
        void Render(const CommandBuffer &) override {}

        [[nodiscard]] Image2d *GetPyramid() const { return pyramid_.get(); }
        // Number of levels in GetPyramid() itself (depth-buffer Hi-Z levels 1..GetLevelCount()).
        [[nodiscard]] uint32_t GetLevelCount() const { return levelCount_; }
        // Bumped every time the pyramid image is (re)created; consumers rewrite their descriptor
        // for GetPyramid()->GetView() when this changes.
        [[nodiscard]] uint64_t GetGeneration() const { return generation_; }

        // 17 would cover a 131072px-wide depth buffer; the pyramid is half-res so 16 is plenty.
        static constexpr uint32_t kMaxLevels = 16;

    private:
        void EnsureSized(UVec2 depthRes);
        void CreatePyramid(UVec2 pyramidRes);
        void CreateMipViews();
        void DestroyMipViews();
        void BuildDescriptors();

        std::unique_ptr<ComputePipeline> downsamplePipeline_;
        std::unique_ptr<Image2d> pyramid_;

        // Per-mip single-level views into pyramid_ (Image2d only exposes one whole-chain view,
        // which can't be bound as a storage image targeting one specific mip).
        std::vector<VkImageView> mipViews_;

        // Allocated once (kMaxLevels) and rewritten on resize: DescriptorSet never frees back to
        // its pool, so re-allocating on every resize would exhaust the pipeline's 64-set pool.
        // levelSets_[0]: binding 0 = gbuf_depth, binding 1 = pyramid mip 0 (storage).
        // levelSets_[k>0]: binding 0 = pyramid mip k-1 (sampled), binding 1 = pyramid mip k.
        std::vector<std::unique_ptr<DescriptorSet>> levelSets_;

        VkDevice device_ = VK_NULL_HANDLE;
        VkImageView lastDepthView_ = VK_NULL_HANDLE;

        uint32_t levelCount_  = 0;
        uint64_t generation_  = 0;
        UVec2 allocatedRes_{0, 0}; // pyramid_'s own resolution (half the depth buffer's)
    };
} // namespace SF::Engine
