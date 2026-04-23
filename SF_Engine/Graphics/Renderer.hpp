#pragma once

#include "PipelineRenderer.hpp"
#include "Stage.hpp"
#include "PipelinePassManager.hpp"

namespace SF::Engine
{
    class Renderer
    {
        friend class RenderSystem;

    public:
        /**
         * Creates a new renderer, fill {@link renderStages} in your subclass of this.
         */
        Renderer() = default;
        virtual ~Renderer() = default;

        virtual void Start() = 0;

        /**
         * Run when updating the renderer manager.
         */
        virtual void Update() = 0;

        /**
         * Checks whether a PipelinePass exists or not.
         * @tparam T The PipelinePass type.
         * @return If the PipelinePass has the System.
         */
        template <typename T>
        bool HasPipelinePass() const
        {
            return PipelinePassManager.Has<T>();
        }

        /**
         * Gets a PipelinePass.
         * @tparam T The PipelinePass type.
         * @return The PipelinePass.
         */
        template <typename T>
        T *GetPipelinePass() const
        {
            return PipelinePassManager.Get<T>();
        }

        /**
         * Adds a PipelinePass.
         * @tparam T The PipelinePass type.
         * @tparam Args The constructor arg types.
         * @param pipelineStage The PipelinePass pipeline stage.
         * @param args The constructor arguments.
         */
        template <typename T, typename... Args>
        T *AddPipelinePass(const Pipeline::Stage &pipelineStage, Args &&...args)
        {
            return PipelinePassManager.Add<T>(
                pipelineStage, std::make_unique<T>(pipelineStage, std::forward<Args>(args)...));
        }

        /**
         * Removes a PipelinePass.
         * @tparam T The PipelinePass type.
         */
        template <typename T>
        void RemovePipelinePass()
        {
            PipelinePassManager.Remove<T>();
        }

        /**
         * Clears all PipelinePasss.
         */
        void ClearPipelinePasss()
        {
            PipelinePassManager.Clear();
        }

        bool IsStarted() const { return started; }
        void SetStarted(bool s) { started = s; }

        RenderStage *GetRenderStage(uint32_t index) const
        {
            if (renderStages.empty() || renderStages.size() < index)
                return nullptr;

            return renderStages.at(index).get();
        }

        void AddRenderStage(std::unique_ptr<RenderStage> &&renderStage)
        {
            renderStages.emplace_back(std::move(renderStage));
        }

    private:
        bool started = false;
        std::vector<std::unique_ptr<RenderStage>> renderStages;
        PipelinePassManager PipelinePassManager;
    };
}