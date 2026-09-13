#include "FrameGraphNode.hpp"
#include <cassert>
#include "EngineRenderpassManager.hpp"

namespace SF::Engine
{
    inline namespace
    {
        [[nodiscard]] bool HasId(const vector<ResourceHandle> &v, ResourceHandle id)
        {
            return ranges::find(v, id) != v.cend();
        }
        [[nodiscard]] bool HasId(const vector<FrameGraphRenderpassNode::AccessDeclaration> &v, ResourceHandle id)
        {
            const auto match = [id](const auto &e) { return e.id == id; };

            return ranges::find_if(v, match) != v.cend();
        }

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
                                                       unique_ptr<EngineRenderpass> &&exec) :
        FrameGraphNode{name, nodeId}, exec{std::move(exec)}
    {
        creates.reserve(10);
        reads.reserve(10);
        writes.reserve(10);
    }

    ResourceHandle FrameGraphRenderpassNode::Read(ResourceHandle id, uint32_t flags)
    {
        assert(!Creates(id) && !Writes(id));
        return Contains(reads, {id, flags}) ? id : reads.emplace_back(AccessDeclaration{.id = id, .flags = flags}).id;
    }

    ResourceHandle FrameGraphRenderpassNode::Write(ResourceHandle id, uint32_t flags)
    {
        return Contains(writes, {id, flags}) ? id : writes.emplace_back(AccessDeclaration{.id = id, .flags = flags}).id;
    }
} // namespace SF::Engine
