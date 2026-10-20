#pragma once

#include <Rendering/RHI/Commands/CommandBuffer.hpp>
#include <Rendering/RHI/Pipelines/Pipeline.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <UtilityClasses/TypeInformation.hpp>
#include "EngineRenderpassInitRegistry.hpp"

namespace SF::Engine
{
    /**
     * @brief Represents a render pipeline that is used to render a type of pipeline.
     */
    class EngineRenderpass : NoCopy
    {
    public:
        /**
         * Creates a new render pipeline.
         * @param stage The stage this renderer will be used in.
         */
        explicit EngineRenderpass(Pipeline::Stage stage) : stage(std::move(stage)) {}

        // virtual ~SFERenderPass() = default; nocopy provides

        /**
         * Called once per frame BEFORE the renderpass for this stage begins.
         * Use for compute dispatches, barriers, or any work that must happen
         * outside a renderpass (e.g. cluster culling). Default: no-op.
         * @input CommandBuffer The open command buffer (outside any renderpass).
         */
        virtual void PreRender(const CommandBuffer &) {}

        /**
         * Runs the render pipeline in the current renderpass.
         * @param commandBuffer The command buffer to record render command into.
         */
        virtual void Render(const CommandBuffer &commandBuffer) = 0;

        [[nodiscard]] const Pipeline::Stage &GetStage() const { return stage; }

        [[nodiscard]] bool IsEnabled() const { return enabled; }
        void SetEnabled(bool enable) { this->enabled = enable; }

        [[nodiscard]] int GetOrder() const { return order; }
        void SetOrder(int o) { order = o; }

    private:
        bool enabled = true;
        Pipeline::Stage stage;
        int order = 0;
    };

    template class TypeInformation<EngineRenderpass>;

    class EngineRenderpassManager : NoCopy
    {
        friend class RenderSystem;

    public:
        /**
         * Checks whether a SFERenderPass exists or not.
         * @tparam T The SFERenderPass type.
         * @return If the SFERenderPass exists.
         */
        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        bool Has() const
        {
            const auto it = SFERenderPasses.find(TypeInfo<EngineRenderpass>::template GetTypeId<T>());
            return it != SFERenderPasses.end() && it->second;
        }

        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        T *Get() const
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            if (auto it = SFERenderPasses.find(typeId); it != SFERenderPasses.end() && it->second)
                return static_cast<T *>(it->second.get());

            return nullptr;
        }

        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        void Remove()
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            RemoveSFERenderPassStage(typeId);
            SFERenderPasses.erase(typeId);
        }

        /**
         * Adds a SFERenderPass.
         * @tparam T The SFERenderPass type.
         * @param stage The SFERenderPass pipeline stage.
         * @param SFERenderPass The SFERenderPass.
         * @return The added renderer.
         */
        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        T *Add(const Pipeline::Stage &stage, std::unique_ptr<T> &&pass)
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            stages.emplace(StageIndex{stage, pass->GetOrder()}, typeId);

            SFERenderPasses[typeId] = std::move(pass);
            return static_cast<T *>(SFERenderPasses[typeId].get());
        }

        /**
         * Clears all SFERenderPasses.
         */
        void Clear();

        /**
         * Runs things idk
         */
        void RunInitCallbacks() { EngineRenderpassInitRegistry::Get().RunAll(*this); }

    private:
        using StageIndex = std::pair<Pipeline::Stage, int>;

        void RemoveSFERenderPassStage(const TypeId &id);

        /**
         * Calls PreRender() on all SFERenderPasses registered for a stage.
         * Must be called BEFORE StartRenderpass for that stage.
         * @param stage The SFERenderPass stage.
         * @param commandBuffer An open command buffer (outside any renderpass).
         */
        void PreRenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /**
         * Iterates through all SFERenderPasses.
         * @param stage The SFERenderPass stage.
         * @param commandBuffer The command buffer to record render command into.
         */
        void RenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /// List of all SFERenderPasses.
        std::unordered_map<TypeId, std::unique_ptr<EngineRenderpass>> SFERenderPasses;
        /// List of SFERenderPass stages.
        std::multimap<StageIndex, TypeId> stages;
    };
} // namespace SF::Engine
