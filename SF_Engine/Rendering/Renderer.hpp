#pragma once

#include "FrameGraph/EngineRenderpassManager.hpp"
#include "FrameGraph/FrameGraph.hpp"
#include "FrameGraph/Stage.hpp"
#include "PipelineRenderer.hpp"

namespace SF::Engine
{
    class Renderer
    {
        friend class RenderSystem;

    public:
        /**
         * Creates a new renderer, fill {@link renderStages} in your subclass of this.
         */
        Renderer()          = default;
        virtual ~Renderer() = default;

        virtual void Start() = 0;

        /**
         * Run when updating the renderer manager.
         */
        virtual void Update() = 0;

        /**
         * Checks whether a SFERenderPass exists or not.
         * @tparam T The SFERenderPass type.
         * @return If the SFERenderPass has the System.
         */
        template<typename T>
        bool HasSFERenderPass() const
        {
            return PassManager.Has<T>();
        }

        /**
         * Gets a SFERenderPass.
         * @tparam T The SFERenderPass type.
         * @return The SFERenderPass.
         */
        template<typename T>
        T *GetSFERenderPass() const
        {
            return PassManager.Get<T>();
        }

        /**
         * Adds a SFERenderPass.
         * @tparam T The SFERenderPass type.
         * @tparam Args The constructor arg types.
         * @param pipelineStage The SFERenderPass pipeline stage.
         * @param args The constructor arguments.
         */
        template<typename T, typename... Args>
        T *AddSFERenderPass(const Pipeline::Stage &pipelineStage, Args &&...args)
        {
            return PassManager.Add<T>(pipelineStage, std::make_unique<T>(pipelineStage, std::forward<Args>(args)...));
        }

        /**
         * Removes a SFERenderPass.
         * @tparam T The SFERenderPass type.
         */
        template<typename T>
        void RemoveSFERenderPass()
        {
            PassManager.Remove<T>();
        }

        /**
         * Clears all SFERenderPasses.
         */
        void ClearSFERenderPasss() { PassManager.Clear(); }

        bool IsStarted() const { return started; }
        void SetStarted(bool s) { started = s; }

        RhiRenderStage *GetRenderStage(uint32_t index) const
        {
            if (renderStages.empty() || renderStages.size() < index)
                return nullptr;

            return renderStages.at(index).get();
        }

        void AddRenderStage(std::unique_ptr<RhiRenderStage> &&renderStage)
        {
            renderStages.emplace_back(std::move(renderStage));
        }

        EngineRenderpassManager *GetSFERenderPassManager() { return &PassManager; }
        /*
                 The frame graph is opt-in per pass: only passes registered via GetFrameGraph().AddPass()
                 are touched by Compile()'s culling/ordering/barriers. A pass never handed to the graph
                 keeps running exactly as EngineRenderpassManager always ran it (enabled, order 0).
                 See SceneRenderer::Start() for which passes are actually wired up and why (buffer-only
                 passes like ClusterCullSFERenderPass are deliberately left out; the graph only models
                 image resources today).
        */
        FrameGraph &GetFrameGraph() { return frameGraph; }

    private:
        bool started = false;
        std::vector<std::unique_ptr<RhiRenderStage>> renderStages;
        EngineRenderpassManager PassManager;
        FrameGraph frameGraph;
    };
} // namespace SF::Engine
