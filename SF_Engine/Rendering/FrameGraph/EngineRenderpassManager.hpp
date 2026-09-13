#pragma once

#include <Rendering/RHI/Commands/CommandBuffer.hpp>
#include <Rendering/RHI/Pipelines/Pipeline.hpp>
#include "EngineRenderpassInitRegistry.hpp"
#include "UtilityClasses/NoCopy.hpp"
#include "UtilityClasses/TypeInformation.hpp"

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

        // virtual ~PipelinePass() = default; nocopy provides

        /**
         * Called once per frame BEFORE the renderpass for this stage begins.
         * Use for compute dispatches, barriers, or any work that must happen
         * outside a renderpass (e.g. cluster culling). Default: no-op.
         * @param commandBuffer The open command buffer (outside any renderpass).
         */
        virtual void PreRender(const CommandBuffer &commandBuffer) {}

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
         * Checks whether a PipelinePass exists or not.
         * @tparam T The PipelinePass type.
         * @return If the PipelinePass exists.
         */
        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        bool Has() const
        {
            const auto it = PipelinePasses.find(TypeInfo<EngineRenderpass>::template GetTypeId<T>());
            return it != PipelinePasses.end() && it->second;
        }

        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        T *Get() const
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            if (auto it = PipelinePasses.find(typeId); it != PipelinePasses.end() && it->second)
                return static_cast<T *>(it->second.get());

            return nullptr;
        }

        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        void Remove()
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            RemovePipelinePassStage(typeId);
            PipelinePasses.erase(typeId);
        }

        /**
         * Adds a PipelinePass.
         * @tparam T The PipelinePass type.
         * @param stage The PipelinePass pipeline stage.
         * @param PipelinePass The PipelinePass.
         * @return The added renderer.
         */
        template<typename T, typename = std::enable_if_t<std::is_convertible_v<T *, EngineRenderpass *>>>
        T *Add(const Pipeline::Stage &stage, std::unique_ptr<T> &&pass)
        {
            const auto typeId = TypeInfo<EngineRenderpass>::template GetTypeId<T>();

            stages.emplace(StageIndex{stage, pass->GetOrder()}, typeId);

            PipelinePasses[typeId] = std::move(pass);
            return static_cast<T *>(PipelinePasses[typeId].get());
        }

        /**
         * Clears all PipelinePasses.
         */
        void Clear();

        /**
         * Runs things idk
         */
        void RunInitCallbacks() { EngineRenderpassInitRegistry::Get().RunAll(*this); }

    private:
        using StageIndex = std::pair<Pipeline::Stage, int>;

        void RemovePipelinePassStage(const TypeId &id);

        /**
         * Calls PreRender() on all PipelinePasses registered for a stage.
         * Must be called BEFORE StartRenderpass for that stage.
         * @param stage The PipelinePass stage.
         * @param commandBuffer An open command buffer (outside any renderpass).
         */
        void PreRenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /**
         * Iterates through all PipelinePasses.
         * @param stage The PipelinePass stage.
         * @param commandBuffer The command buffer to record render command into.
         */
        void RenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /// List of all PipelinePasses.
        std::unordered_map<TypeId, std::unique_ptr<EngineRenderpass>> PipelinePasses;
        /// List of PipelinePass stages.
        std::multimap<StageIndex, TypeId> stages;
    };
} // namespace SF::Engine
