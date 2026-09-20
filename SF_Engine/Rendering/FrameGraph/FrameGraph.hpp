#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Math/Vectors/Vector.hpp>
#include <volk.h>

#include <UtilityClasses/NoCopy.hpp>
#include "EngineRenderpassManager.hpp"
#include "FrameGraphNode.hpp"

namespace SF::Engine
{
    enum class ResourceUsage
    {
        None,
        ColorAttachment,
        DepthStencilAttachment,
        SampledTexture, // Shader Read
        StorageRead,    // Compute Read
        StorageWrite    // Compute Write
    };

    struct Texture
    {
        VkImage image    = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format  = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    struct ResourceState
    {
        VkImageLayout current_layout     = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 last_stage = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 last_access       = VK_ACCESS_2_NONE;

        bool operator==(const ResourceState &) const = default;
    };

    using VulkanStateMapping = ResourceState;

    inline VulkanStateMapping GetVulkanState(ResourceUsage usage)
    {
        switch (usage)
        {
            case ResourceUsage::ColorAttachment:
                return {.current_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .last_stage     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .last_access    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
            case ResourceUsage::DepthStencilAttachment:
                return {.current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        .last_stage     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        .last_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
            case ResourceUsage::SampledTexture:
                return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT};
            case ResourceUsage::StorageRead:
                return {VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT};
            case ResourceUsage::StorageWrite:
                return {VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_WRITE_BIT};
            default:
                return {.current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .last_stage     = VK_PIPELINE_STAGE_2_NONE,
                        .last_access    = VK_ACCESS_2_NONE};
        }
    }

    /**
     * @brief A resource-dependency graph laid over EngineRenderpassManager's existing passes.
     *
     * WHAT THIS DOES:
     *   - Passes declare, once, which named resources they Create/Read/Write and with what
     *     ResourceUsage.
     *   - Compile() culls passes nothing reads (dead code elimination for renderer features
     *     that are toggled off -- e.g. an SSR pass nobody samples this frame), topologically
     *     sorts the survivors, and writes the result back onto the real EngineRenderpass
     *     objects via SetEnabled()/SetOrder(). EngineRenderpassManager's existing
     *     PreRenderStage()/RenderStage() iteration is untouched and just picks up the new
     *     order/enabled-state for free.
     *   - Compile() also detects write-after-write-without-a-read hazards: this is precisely
     *     the atmosphere/hdr bug class (a compute PreRender() write silently discarded by a
     *     later renderpass's LOAD_OP_CLEAR) and gets logged as a warning naming both passes.
     *   - EmitPendingBarriers() hands back a ready-to-record batch of VkImageMemoryBarrier2s
     *     for a given render-stage index, computed once at Compile() time.
     *
     * WHAT THIS DELIBERATELY DOES NOT DO:
     *   - It does not call PreRender()/Render() itself, and does not touch RenderSystem.cpp's
     *     render loop. Every EngineRenderpass's actual Vulkan work still has to run inside the
     *     matching (renderpass, subpass) boundary that RenderSystem.cpp already manages, so
     *     there's no safe way for a general resource graph to also own dispatching those calls
     *     without either duplicating that loop or editing it. See FrameGraph/README notes
     *     (bottom of this file's comment, or the chat message this shipped with) for the exact
     *     one-line hookup instead.
     *   - It does not insert barriers automatically mid-frame. Vulkan barriers can only be
     *     recorded outside an active render pass, and PreRenderStage() is the only point in the
     *     existing loop that's guaranteed to run before a renderpass begins -- so a pass has to
     *     explicitly call EmitPendingBarriers() from there. This is a deliberate one-line manual
     *     hook rather than an automatic one threaded through the hot render loop.
     */
    class FrameGraph : NoCopy
    {
    public:
        FrameGraph() = default;

        // ---- Resources ----

        /**
         * Registers a named, externally-owned resource (an attachment, an offscreen Image2d, an
         * ImageDepth, a baked LUT...). `resolve` is called lazily, only when the graph actually
         * needs to build a barrier for it, so it's safe to import a resource before the
         * underlying Vulkan image exists yet (e.g. before the first ResetRenderStages()) -- if
         * `resolve` returns a Texture with a VK_NULL_HANDLE image, that access is skipped for
         * this Compile() rather than producing a bogus barrier.
         *
         * @param initialState What the resource's layout/stage/access already is when the graph
         *        first touches it (e.g. a baked LUT already sitting in
         *        SHADER_READ_ONLY_OPTIMAL). Defaults to UNDEFINED, appropriate for anything the
         *        graph itself is responsible for creating/clearing first.
         */
        ResourceHandle ImportResource(std::string name, std::function<Texture()> resolve,
                                      ResourceState initialState = {});

        [[nodiscard]] ResourceHandle FindResource(std::string_view name) const;
        [[nodiscard]] ResourceHandle RequireResource(std::string_view name) const;

        // ---- Passes ----

        /**
         * Wraps an already-registered EngineRenderpass (owned by EngineRenderpassManager) as a
         * graph node. Call once per pass -- right after EngineRenderpassManager::Add<T>() is a
         * natural spot -- then declare its resource dependencies with Read/Write/Create before
         * calling Compile(). Returns a reference valid for the lifetime of the FrameGraph.
         */
        FrameGraphRenderpassNode &AddPass(std::string_view name, EngineRenderpass &pass);

        // Declares that `node` reads `resource` as `usage`, whether that happens in the pass's
        // PreRender() (compute) or Render() (subpass) step.
        void Read(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage);

        // Declares that `node` writes `resource` as `usage`, where the resource already has
        // meaningful contents from an earlier pass this frame (so Compile()'s hazard detector
        // treats this as consuming, not stomping, whatever came before -- pair it with a Read of
        // the same resource on this node if this pass also reads it back).
        void Write(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage);

        // Declares that `node` is the FIRST writer of `resource` this frame: it fully
        // (re)initializes the resource and doesn't care about, or intentionally discards,
        // whatever was there before (a render target being cleared, a compute dispatch that
        // fully overwrites a buffer). Distinguishing Create from Write is what lets Compile()'s
        // hazard detector flag "X wrote resource R, then Y re-created R with nothing ever
        // reading X's output in between" -- exactly the atmosphere/hdr bug shape.
        void Create(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage);

        // Marks a node as always executing regardless of whether anything reads its outputs --
        // use for anything with an externally-visible effect: final composite/present passes,
        // GPU readback, debug overlays.
        void MarkSideEffect(FrameGraphRenderpassNode &node);

        // ---- Compile / Execute ----

        /**
         * Culls dead passes, topologically sorts the survivors, logs any write-after-write
         * hazards, and rebuilds the pending barrier plan. Call once after registering all
         * passes and their dependencies, and again any time a pass is added/removed or its
         * dependencies change at runtime (e.g. a graphics-settings toggle that adds/removes a
         * whole pipeline pass). Cheap enough to not worry about beyond that -- it's O(passes +
         * accesses), no allocdriven-per-frame cost, which is what makes Execute-side
         * (EmitPendingBarriers) fast: it's just replaying a precomputed list.
         */
        void Compile();

        /**
         * Records the batch of image barriers needed before `renderStageIndex`'s renderpass
         * begins (i.e. before RenderSystem::StartRenderpass() is called for that render stage
         * this frame). Call this from the LAST PreRender() that runs for that render stage --
         * concretely, the pass registered at that render stage's highest subpass index that has
         * a PreRender(). Safe to call every frame; it's just replaying the plan Compile() built.
         */
        void EmitPendingBarriers(uint32_t renderStageIndex, const CommandBuffer &commandBuffer) const;

        [[nodiscard]] bool IsCompiled() const { return compiled; }
        void Invalidate() { compiled = false; }

        [[nodiscard]] size_t GetPassCount() const { return nodes.size(); }
        [[nodiscard]] size_t GetSurvivingPassCount() const { return order.size(); }

    private:
        struct ResourceEntry
        {
            std::string name;
            std::function<Texture()> resolve;
            ResourceState state{}; // persists across frames/Compile() calls
        };

        struct PendingBarrier
        {
            uint32_t renderStageIndex;
            VkImageMemoryBarrier2 barrier;
        };

        // Shared by Cull()/TopoSort(): for each node, which OTHER node indices it must run
        // after, derived from "the last Create/Write of a resource before this Read of it, in
        // declaration order". The graph doesn't version resources, so a Read always binds to
        // whichever Create/Write for that handle was declared most recently before it -- if two
        // passes are meant to write the same resource independently of each other's timing,
        // give them separate resource handles.
        [[nodiscard]] std::vector<std::vector<uint32_t>> BuildDependencyEdges() const;

        void Cull(const std::vector<std::vector<uint32_t>> &dependsOn);
        void TopoSort(const std::vector<std::vector<uint32_t>> &dependsOn);
        void DetectHazards() const;
        void BuildBarrierPlan();

        // deque, not vector: AddPass() returns a reference the caller uses immediately
        // afterward to declare Read/Write/Create. A vector reallocating on a later AddPass()
        // call would silently invalidate that reference; a deque never does on back-insertion
        // (same reasoning as DescriptorSetWrites::bufferInfos in DescriptorSetBuilder.hpp).
        std::deque<FrameGraphRenderpassNode> nodes;
        std::vector<ResourceEntry> resources;
        std::unordered_map<std::string, ResourceHandle> resourcesByName;

        std::vector<uint32_t> order; // node indices, post-Compile() execution order
        std::vector<PendingBarrier> pendingBarriers;

        bool compiled = false;
    };
} // namespace SF::Engine
