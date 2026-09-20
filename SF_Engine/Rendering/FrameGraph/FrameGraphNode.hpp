#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    class EngineRenderpass;

    using ResourceHandle = uint32_t;

    class FrameGraphNode
    {
        friend class FrameGraph;

    public:
        FrameGraphNode()                           = delete;
        FrameGraphNode(const FrameGraphNode &)     = delete;
        FrameGraphNode(FrameGraphNode &&) noexcept = default;
        virtual ~FrameGraphNode()                  = default;

        FrameGraphNode &operator=(const FrameGraphNode &)     = delete;
        FrameGraphNode &operator=(FrameGraphNode &&) noexcept = delete;

        [[nodiscard]] auto Id() const { return id; }
        [[nodiscard]] string_view Name() const { return name; }
        [[nodiscard]] auto RefCount() const { return refCount; }

    protected:
        FrameGraphNode(const string_view name, const uint32_t id) : name{name}, id{id} {}

    private:
        string name;
        const uint32_t id;
        int32_t refCount{0};
    };

    /**
     * @brief A FrameGraph node wrapping a single EngineRenderpass. Declares which named
     * resources the wrapped pass creates/reads/writes so FrameGraph::Compile() can cull it
     * when nothing downstream needs its output, order it correctly relative to its
     * dependencies, and figure out what barriers need to run before it.
     */
    class FrameGraphRenderpassNode final : public FrameGraphNode
    {
        friend class FrameGraph;

    public:
        FrameGraphRenderpassNode(const FrameGraphRenderpassNode &)     = delete;
        FrameGraphRenderpassNode(FrameGraphRenderpassNode &&) noexcept = default;

        FrameGraphRenderpassNode &operator=(const FrameGraphRenderpassNode &)     = delete;
        FrameGraphRenderpassNode &operator=(FrameGraphRenderpassNode &&) noexcept = delete;

        struct AccessDeclaration
        {
            ResourceHandle id;
            uint32_t flags; // a SF::Engine::ResourceUsage (see FrameGraph.hpp), stored as a
                            // plain uint32_t here so this header doesn't need to depend on it.

            bool operator==(const AccessDeclaration &) const = default;
        };

        [[nodiscard]] bool Creates(ResourceHandle id) const;
        [[nodiscard]] bool Reads(ResourceHandle id) const;
        [[nodiscard]] bool Writes(ResourceHandle id) const;

        [[nodiscard]] auto HasSideEffect() const { return hasSideEffect; }
        [[nodiscard]] auto CanExecute() const { return RefCount() > 0 || HasSideEffect(); }
        [[nodiscard]] bool IsCulled() const { return culled; }

        [[nodiscard]] EngineRenderpass *GetPass() const { return pass; }

        // Tag-dispatch accessors for iterating this node's declarations, e.g.:
        //   for (const auto &r : node.each(FrameGraphRenderpassNode::Read{})) ...
        //
        // NOTE: these tag types intentionally do NOT share a name with the private
        // AddRead/AddWrite/AddCreate mutators below. A nested type and a member function of
        // the same class CAN share a spelling for *unqualified* lookup from inside the class,
        // but `Node::Read{}` from calling code outside the class resolves to the function
        // (hiding the type) and fails to compile -- so keeping these distinct isn't just
        // style, it's required for FrameGraph.cpp to actually be able to call each().
        struct Create
        {
        };
        [[nodiscard]] decltype(auto) each(const Create) const { return creates; }

        struct Read
        {
        };
        [[nodiscard]] decltype(auto) each(const Read) const { return reads; }

        struct Write
        {
        };
        [[nodiscard]] decltype(auto) each(const Write) const { return writes; }

    private:
        // Non-owning: the real EngineRenderpass is (and must stay) owned by
        // EngineRenderpassManager, via the unique_ptr handed to EngineRenderpassManager::Add<T>().
        // The frame graph only needs to declare dependencies for it and flip
        // SetEnabled()/SetOrder() at Compile() time -- it has no business co-owning the pass;
        // two owners of the same object is a lifetime bug waiting to happen.
        FrameGraphRenderpassNode(const string_view name, uint32_t nodeId, EngineRenderpass *pass);

        ResourceHandle AddRead(ResourceHandle id, uint32_t flags);
        ResourceHandle AddWrite(ResourceHandle id, uint32_t flags);
        ResourceHandle AddCreate(ResourceHandle id, uint32_t flags);

        void MarkSideEffect() { hasSideEffect = true; }
        void SetCulled(bool value) { culled = value; }

        EngineRenderpass *pass;

        vector<AccessDeclaration> creates;
        vector<AccessDeclaration> reads;
        vector<AccessDeclaration> writes;

        bool hasSideEffect{false};
        bool culled{false};
        bool OutOfView{false}; // reserved for future occlusion culling
    };
} // namespace SF::Engine
