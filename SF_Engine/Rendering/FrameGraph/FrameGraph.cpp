#include "FrameGraph.hpp"

#include <algorithm>
#include <cassert>
#include <set>

#include <Engine/Log/Log.hpp>

namespace SF::Engine
{
    ResourceHandle FrameGraph::ImportResource(std::string name, std::function<Texture()> resolve,
                                              ResourceState initialState)
    {
        assert(resourcesByName.find(name) == resourcesByName.end() &&
               "FrameGraph::ImportResource: a resource with this name is already registered");

        const auto handle = static_cast<ResourceHandle>(resources.size());
        resourcesByName.emplace(name, handle);
        resources.push_back(ResourceEntry{std::move(name), std::move(resolve), initialState});
        return handle;
    }

    ResourceHandle FrameGraph::FindResource(std::string_view name) const
    {
        // std::string(name) rather than a transparent-hash lookup: this only ever runs at
        // graph-setup time (a handful of calls total), never per frame, so the extra
        // allocation is a non-issue and not worth widening the map's hasher/comparator for.
        if (const auto it = resourcesByName.find(std::string(name)); it != resourcesByName.end())
            return it->second;
        return static_cast<ResourceHandle>(-1);
    }

    ResourceHandle FrameGraph::RequireResource(std::string_view name) const
    {
        const auto handle = FindResource(name);
        assert(handle != static_cast<ResourceHandle>(-1) &&
               "FrameGraph::RequireResource: no resource registered with this name");
        return handle;
    }

    FrameGraphRenderpassNode &FrameGraph::AddPass(std::string_view name, EngineRenderpass &pass)
    {
        const auto id = static_cast<uint32_t>(nodes.size());

        // Constructed as a named local, then moved in, rather than nodes.emplace_back(...)
        // directly: FrameGraphRenderpassNode's constructor is private (only FrameGraph is a
        // friend), and access control for a constructor invoked deep inside
        // std::deque::emplace_back is checked against libstdc++'s own internals, not against
        // FrameGraph, so emplace_back can't call it even with the friend declaration. A direct
        // construction right here, in FrameGraph's own code, is a context the friendship
        // actually covers; push_back then only needs the public move constructor.
        FrameGraphRenderpassNode temp(name, id, &pass);
        nodes.push_back(std::move(temp));
        return nodes.back();
    }

    void FrameGraph::Read(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage)
    {
        node.AddRead(resource, static_cast<uint32_t>(usage));
    }

    void FrameGraph::Write(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage)
    {
        node.AddWrite(resource, static_cast<uint32_t>(usage));
    }

    void FrameGraph::Create(FrameGraphRenderpassNode &node, ResourceHandle resource, ResourceUsage usage)
    {
        node.AddCreate(resource, static_cast<uint32_t>(usage));
    }

    void FrameGraph::MarkSideEffect(FrameGraphRenderpassNode &node) { node.MarkSideEffect(); }

    std::vector<std::vector<uint32_t>> FrameGraph::BuildDependencyEdges() const
    {
        std::vector<std::vector<uint32_t>> dependsOn(nodes.size());
        std::unordered_map<ResourceHandle, uint32_t> lastWriter;

        for (uint32_t i = 0; i < nodes.size(); ++i)
        {
            const auto &node = nodes[i];

            // A Write depends on whatever produced the resource before it, same as a Read does:
            // most passes in this engine accumulate onto a render target via blend state (GBuffer
            // -> DeferredLight -> Atmosphere composite all draw onto "hdr" without ever sampling
            // it as a texture), which is exactly as real a data dependency on the prior pass as an
            // explicit shader read would be. Only Create is exempt. It explicitly means "I don't
            // care what was here", so it doesn't chain onto a prior producer.
            for (const auto &r: node.each(FrameGraphRenderpassNode::Read{}))
                if (const auto it = lastWriter.find(r.id); it != lastWriter.end())
                    dependsOn[i].push_back(it->second);
            for (const auto &w: node.each(FrameGraphRenderpassNode::Write{}))
                if (const auto it = lastWriter.find(w.id); it != lastWriter.end())
                    dependsOn[i].push_back(it->second);

            for (const auto &c: node.each(FrameGraphRenderpassNode::Create{}))
                lastWriter[c.id] = i;
            for (const auto &w: node.each(FrameGraphRenderpassNode::Write{}))
                lastWriter[w.id] = i;
        }

        return dependsOn;
    }

    void FrameGraph::Cull(const std::vector<std::vector<uint32_t>> &dependsOn)
    {
        const auto n = static_cast<uint32_t>(nodes.size());

        std::vector<bool> necessary(n, false);
        std::vector<uint32_t> stack;

        for (uint32_t i = 0; i < n; ++i)
        {
            nodes[i].refCount = 0;
            if (nodes[i].HasSideEffect())
            {
                necessary[i] = true;
                stack.push_back(i);
            }
        }

        // Backward reachability from every side-effect node: anything a necessary node
        // transitively depends on is necessary too. refCount is bumped on every traversed edge
        // (not just the first time a dependency is discovered) so it ends up as an accurate
        // "how many surviving passes actually read this pass's output" diagnostic count,
        // matching what FrameGraphNode::RefCount()/CanExecute() expect.
        while (!stack.empty())
        {
            const uint32_t i = stack.back();
            stack.pop_back();

            for (const uint32_t dep: dependsOn[i])
            {
                if (!necessary[dep])
                {
                    necessary[dep] = true;
                    stack.push_back(dep);
                }
                nodes[dep].refCount++;
            }
        }

        for (uint32_t i = 0; i < n; ++i)
        {
            nodes[i].SetCulled(!necessary[i]);
            nodes[i].GetPass()->SetEnabled(necessary[i]);
        }
    }

    void FrameGraph::TopoSort(const std::vector<std::vector<uint32_t>> &dependsOn)
    {
        const auto n = static_cast<uint32_t>(nodes.size());

        std::vector<uint32_t> indegree(n, 0);
        std::vector<std::vector<uint32_t>> dependents(n);

        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].IsCulled())
                continue;

            // Every dependency of a surviving node is itself necessarily surviving -- Cull()
            // marks a node's entire transitive dependency closure as necessary, so this can't
            // point at a culled node.
            for (const uint32_t d: dependsOn[i])
            {
                dependents[d].push_back(i);
                indegree[i]++;
            }
        }

        // Kahn's algorithm, with the "ready" set ordered by declaration index so that, among
        // several passes that could legally run next, the one declared earliest wins -- keeps
        // the compiled order stable and predictable rather than depending on hash/insertion
        // iteration order.
        std::set<uint32_t> ready;
        for (uint32_t i = 0; i < n; ++i)
            if (!nodes[i].IsCulled() && indegree[i] == 0)
                ready.insert(i);

        order.clear();
        order.reserve(n);

        while (!ready.empty())
        {
            const uint32_t i = *ready.begin();
            ready.erase(ready.begin());
            order.push_back(i);

            for (const uint32_t dependent: dependents[i])
                if (--indegree[dependent] == 0)
                    ready.insert(dependent);
        }

        const auto survivingCount = static_cast<size_t>(
                std::count_if(nodes.begin(), nodes.end(), [](const auto &node) { return !node.IsCulled(); }));

        if (order.size() != survivingCount)
            Log::Error("FrameGraph::Compile: cycle detected among surviving passes ({} of {} passes could be "
                       "ordered) -- two passes are declared as reading resources the other one writes. Execution "
                       "order was left partial; the un-orderable passes were NOT disabled, so they'll still run, "
                       "just in whatever order EngineRenderpassManager's stage/order tiebreak picks.",
                       order.size(), survivingCount);

        for (uint32_t rank = 0; rank < order.size(); ++rank)
            nodes[order[rank]].GetPass()->SetOrder(static_cast<int>(rank));
    }

    void FrameGraph::DetectHazards() const
    {
        // Only a Create is ever flagged as a potential stomp: a Write is a read-modify-write (it
        // depends on and consumes whatever came before, per BuildDependencyEdges above), so a
        // Create->Write->Write chain is completely normal accumulation and never a hazard. What's
        // NOT normal is a Create landing on a resource that something already Wrote (or Created)
        // this frame without that earlier output ever being consumed -- that's a silent stomp,
        // and it's exactly the atmosphere/hdr bug shape: a compute PreRender() Write into "hdr",
        // then the renderpass's implicit clear (a Create, from the graph's point of view) wiping
        // it before anything read it.
        struct LastProducer
        {
            uint32_t nodeIdx;
            bool consumedSince;
        };

        std::unordered_map<ResourceHandle, LastProducer> last;

        // Deliberately walks raw declaration order (0..nodes.size()), NOT the post-cull `order`
        // list -- this has to run and warn regardless of whether Cull() would also end up
        // disabling the offending pass. A write that's immediately clobbered with nothing
        // consuming it is exactly the kind of thing Cull() correctly (and silently) turns off,
        // which is the right call for saving the wasted GPU work, but means the warning would
        // never fire if this only looked at survivors -- and "why did my pass stop running" is a
        // much worse debugging experience than a warning explaining the mis-wiring up front.
        for (uint32_t idx = 0; idx < nodes.size(); ++idx)
        {
            const auto &node = nodes[idx];

            for (const auto &r: node.each(FrameGraphRenderpassNode::Read{}))
                if (const auto it = last.find(r.id); it != last.end())
                    it->second.consumedSince = true;
            for (const auto &w: node.each(FrameGraphRenderpassNode::Write{}))
                if (const auto it = last.find(w.id); it != last.end())
                    it->second.consumedSince = true;

            for (const auto &c: node.each(FrameGraphRenderpassNode::Create{}))
            {
                if (const auto it = last.find(c.id); it != last.end() && !it->second.consumedSince)
                {
                    Log::Warning("FrameGraph: '{}' (re)creates resource '{}', discarding whatever '{}' left there -- "
                                 "nothing ever read or wrote (i.e. consumed) '{}'s output first, so that work is "
                                 "wasted at best. This is exactly the atmosphere/hdr clear-after-compute bug shape: a "
                                 "renderpass clearing an attachment a compute pass just wrote into, with nothing "
                                 "consuming the compute output first.",
                                 nodes[idx].Name(), resources[c.id].name, nodes[it->second.nodeIdx].Name(),
                                 nodes[it->second.nodeIdx].Name());
                }
                last[c.id] = {idx, false};
            }

            for (const auto &w: node.each(FrameGraphRenderpassNode::Write{}))
                if (!last.contains(w.id))
                    last[w.id] = {idx, false};
        }
    }

    void FrameGraph::BuildBarrierPlan()
    {
        // Two passes over `order`, both updating resources[].state via the exact same
        // transitions, because every transition sets state unconditionally to
        // GetVulkanState(usage) regardless of what it was before, the state a resource ends
        // this frame in is a fixed point independent of where the pass started. Pass 1 finds
        // that fixed point (the true "end of last frame" state, since this same plan repeats
        // every frame); pass 2 replays the identical sequence starting from it and records the
        // real barrier list, so the very first access of a persistent resource each frame gets
        // a correct srcStage/srcAccess instead of an assumed-UNDEFINED one. See the frame graph
        // hookup notes for the worked example (this is what makes cross-frame WAR hazards on
        // reused resources like atmoColorRT_-style ping-pong targets come out correct).
        for (int pass = 0; pass < 2; ++pass)
        {
            const bool recording = (pass == 1);
            if (recording)
                pendingBarriers.clear();

            for (const uint32_t idx: order)
            {
                const auto &node          = nodes[idx];
                const uint32_t stageIndex = node.GetPass()->GetStage().first;

                const auto transition = [&](ResourceHandle id, ResourceUsage usage)
                {
                    auto &res         = resources[id];
                    const auto target = GetVulkanState(usage);

                    if (res.state == target)
                        return;

                    if (recording)
                    {
                        if (const Texture tex = res.resolve ? res.resolve() : Texture{}; tex.image != VK_NULL_HANDLE)
                        {
                            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                            barrier.srcStageMask        = res.state.last_stage;
                            barrier.srcAccessMask       = res.state.last_access;
                            barrier.dstStageMask        = target.last_stage;
                            barrier.dstAccessMask       = target.last_access;
                            barrier.oldLayout           = res.state.current_layout;
                            barrier.newLayout           = target.current_layout;
                            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.image               = tex.image;
                            barrier.subresourceRange    = {tex.aspect_mask, 0, VK_REMAINING_MIP_LEVELS, 0,
                                                           VK_REMAINING_ARRAY_LAYERS};

                            pendingBarriers.push_back(PendingBarrier{stageIndex, barrier});
                        }
                        // else: resource not resolvable yet (e.g. before the first
                        // ResetRenderStages()) -- skip silently rather than record a barrier
                        // against a null image; state is still advanced below so later
                        // transitions in this same pass stay consistent.
                    }

                    res.state = target;
                };

                for (const auto &[id, flags]: node.each(FrameGraphRenderpassNode::Read{}))
                    transition(id, static_cast<ResourceUsage>(flags));
                for (const auto &[id, flags]: node.each(FrameGraphRenderpassNode::Create{}))
                    transition(id, static_cast<ResourceUsage>(flags));
                for (const auto &[id, flags]: node.each(FrameGraphRenderpassNode::Write{}))
                    transition(id, static_cast<ResourceUsage>(flags));
            }
        }
    }

    void FrameGraph::Compile()
    {
        const auto dependsOn = BuildDependencyEdges();
        DetectHazards();
        Cull(dependsOn);
        TopoSort(dependsOn);
        BuildBarrierPlan();

        compiled = true;
    }

    void FrameGraph::EmitPendingBarriers(uint32_t renderStageIndex, const CommandBuffer &commandBuffer) const
    {
        std::vector<VkImageMemoryBarrier2> batch;
        for (const auto &pending: pendingBarriers)
            if (pending.renderStageIndex == renderStageIndex)
                batch.push_back(pending.barrier);

        if (batch.empty())
            return;

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(batch.size());
        dependencyInfo.pImageMemoryBarriers    = batch.data();

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
} // namespace SF::Engine
