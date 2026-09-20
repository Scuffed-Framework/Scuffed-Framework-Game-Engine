#include "FrameGraphNode.hpp"
#include <algorithm>
#include <cassert>
#include "EngineRenderpassManager.hpp"

namespace SF::Engine
{
    inline namespace
    {
        // Matches by resource id alone -- used by the public Reads()/Writes()/Creates() queries,
        // where "does this pass touch resource X at all" shouldn't care which usage flags it
        // was declared with.
        [[nodiscard]] bool HasId(const vector<FrameGraphRenderpassNode::AccessDeclaration> &v, ResourceHandle id)
        {
            const auto match = [id](const auto &e) { return e.id == id; };

            return ranges::find_if(v, match) != v.cend();
        }

        // Matches the full (id, flags) tuple -- used by the AddRead/AddWrite/AddCreate mutators
        // to dedupe an exact re-declaration while still allowing the same resource to appear
        // twice with genuinely different usage flags.
        [[nodiscard]] bool Contains(const vector<FrameGraphRenderpassNode::AccessDeclaration> &v,
                                    FrameGraphRenderpassNode::AccessDeclaration n)
        {
            return ranges::find(v, n) != v.cend();
        }
    } // namespace

    bool FrameGraphRenderpassNode::Creates(ResourceHandle id) const { return HasId(creates, id); }
    bool FrameGraphRenderpassNode::Reads(ResourceHandle id) const { return HasId(reads, id); }
    bool FrameGraphRenderpassNode::Writes(ResourceHandle id) const { return HasId(writes, id); }


    FrameGraphRenderpassNode::FrameGraphRenderpassNode(const string_view name, uint32_t nodeId,
                                                       EngineRenderpass *pass) :
        FrameGraphNode{name, nodeId}, pass{pass}
    {
        assert(pass && "FrameGraphRenderpassNode: pass must already be registered with "
                       "EngineRenderpassManager before being wrapped in a graph node.");
        creates.reserve(4);
        reads.reserve(10);
        writes.reserve(10);
    }

    ResourceHandle FrameGraphRenderpassNode::AddRead(ResourceHandle id, uint32_t flags)
    {
        assert(!Creates(id) && !Writes(id) &&
               "FrameGraphRenderpassNode: a pass reading a resource it also creates/writes this "
               "frame should just declare the Write/Create with the read-relevant usage; a "
               "separate Read of the same handle would be a self-dependency the graph can't order.");
        return Contains(reads, {id, flags}) ? id : reads.emplace_back(AccessDeclaration{.id = id, .flags = flags}).id;
    }

    ResourceHandle FrameGraphRenderpassNode::AddWrite(ResourceHandle id, uint32_t flags)
    {
        return Contains(writes, {id, flags}) ? id
                                              : writes.emplace_back(AccessDeclaration{.id = id, .flags = flags}).id;
    }

    ResourceHandle FrameGraphRenderpassNode::AddCreate(ResourceHandle id, uint32_t flags)
    {
        return Contains(creates, {id, flags}) ? id
                                               : creates.emplace_back(AccessDeclaration{.id = id, .flags = flags}).id;
    }
} // namespace SF::Engine
