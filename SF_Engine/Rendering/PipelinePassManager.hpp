#pragma once

#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Pipelines/Pipeline.hpp>
#include "UtilityClasses/NoCopy.hpp"
#include "UtilityClasses/TypeInformation.hpp"
#include "PipelinePassInit.hpp"

namespace SF::Engine
{
    /**
     * @brief Represents a render pipeline that is used to render a type of pipeline.
     */
    class PipelinePass : NoCopy
    {
    public:
        /**
         * Creates a new render pipeline.
         * @param stage The stage this renderer will be used in.
         */
        explicit PipelinePass(Pipeline::Stage stage) : stage(std::move(stage)) {}

        virtual ~PipelinePass() = default;

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

        const Pipeline::Stage &GetStage() const
        {
            return stage;
        }

        bool IsEnabled() const
        {
            return enabled;
        }
        void SetEnabled(bool enable)
        {
            this->enabled = enable;
        }

        int GetOrder() const { return order; }
        void SetOrder(int o) { order = o; }

    private:
        bool enabled = true;
        Pipeline::Stage stage;
        int order = 0;
    };

    template class TypeInformation<PipelinePass>;

    class PipelinePassManager : NoCopy
    {
        friend class RenderSystem;

    public:
        /**
         * Checks whether a PipelinePass exists or not.
         * @tparam T The PipelinePass type.
         * @return If the PipelinePass exists.
         */
        template <typename T, typename = std::enable_if_t<std::is_convertible_v<T *, PipelinePass *>>>
        bool Has() const
        {
            const auto it = PipelinePasss.find(TypeInfo<PipelinePass>::template GetTypeId<T>());
            return it != PipelinePasss.end() && it->second;
        }

        template <typename T, typename = std::enable_if_t<std::is_convertible_v<T *, PipelinePass *>>>
        T *Get() const
        {
            const auto typeId = TypeInfo<PipelinePass>::template GetTypeId<T>();

            if (auto it = PipelinePasss.find(typeId); it != PipelinePasss.end() && it->second)
                return static_cast<T *>(it->second.get());

            return nullptr;
        }

        template <typename T, typename = std::enable_if_t<std::is_convertible_v<T *, PipelinePass *>>>
        void Remove()
        {
            const auto typeId = TypeInfo<PipelinePass>::template GetTypeId<T>();

            RemovePipelinePassStage(typeId);
            PipelinePasss.erase(typeId);
        }

        /**
         * Adds a PipelinePass.
         * @tparam T The PipelinePass type.
         * @param stage The PipelinePass pipeline stage.
         * @param PipelinePass The PipelinePass.
         * @return The added renderer.
         */
        template <typename T, typename = std::enable_if_t<std::is_convertible_v<T *, PipelinePass *>>>
        T *Add(const Pipeline::Stage &stage, std::unique_ptr<T> &&pass)
        {
            const auto typeId = TypeInfo<PipelinePass>::template GetTypeId<T>();

            stages.emplace(StageIndex{stage, pass->GetOrder()}, typeId);

            PipelinePasss[typeId] = std::move(pass);
            return static_cast<T *>(PipelinePasss[typeId].get());
        }

        /**
         * Clears all PipelinePasss.
         */
        void Clear();

        /**
         * Runs things idk
         */
        void RunInitCallbacks()
        {
            PipelinePassInitRegistry::Get().RunAll(*this);
        }

    private:
        using StageIndex = std::pair<Pipeline::Stage, int>;

        void RemovePipelinePassStage(const TypeId &id);

        /**
         * Calls PreRender() on all PipelinePasss registered for a stage.
         * Must be called BEFORE StartRenderpass for that stage.
         * @param stage The PipelinePass stage.
         * @param commandBuffer An open command buffer (outside any renderpass).
         */
        void PreRenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /**
         * Iterates through all PipelinePasss.
         * @param stage The PipelinePass stage.
         * @param commandBuffer The command buffer to record render command into.
         */
        void RenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer);

        /// List of all PipelinePasss.
        std::unordered_map<TypeId, std::unique_ptr<PipelinePass>> PipelinePasss;
        /// List of PipelinePass stages.
        std::multimap<StageIndex, TypeId> stages;
    };
}
