#pragma once
#include <cstdint>
#include <memory>
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
            uint32_t flags;

            bool operator==(const AccessDeclaration &) const = default;
        };

        [[nodiscard]] bool Creates(ResourceHandle id) const;
        [[nodiscard]] bool Reads(ResourceHandle id) const;
        [[nodiscard]] bool Writes(ResourceHandle id) const;

        [[nodiscard]] auto HasSideEffect() const { return hasSideEffect; }
        [[nodiscard]] auto CanExecute() const { return RefCount() > 0 || HasSideEffect(); }

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
        FrameGraphRenderpassNode(const string_view name, uint32_t nodeId, unique_ptr<EngineRenderpass> &&);

        ResourceHandle Read(ResourceHandle id, uint32_t flags);
        [[nodiscard]] ResourceHandle Write(ResourceHandle id, uint32_t flags);

        unique_ptr<EngineRenderpass> exec;

        vector<ResourceHandle> creates;
        vector<AccessDeclaration> reads;
        vector<AccessDeclaration> writes;

        bool hasSideEffect{false};
        bool OutOfView{false}; // occlusion culling
    };
} // namespace SF::Engine
