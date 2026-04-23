#pragma once

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>

// Always use the GLFW backend : it auto-installs all input callbacks and works
// on every platform (including Windows). The Win32 backend is only needed when
// GLFW is not in use.
#include <ImGui/ocornut/imgui.h>
#include <ImGui/ocornut/imgui_impl_glfw.h>
#include <ImGui/ocornut/imgui_impl_vulkan.h>

#include <functional>

namespace SF::Engine
{
    /**
     * @brief PipelinePass that integrates Dear ImGui into the SF render pipeline.
     *
     * Setup
     * -----
     * Add this PipelinePass LAST within a subpass so it composites on top:
     *
     *     AddPipelinePass<ImGuiPipelinePass>(Pipeline::Stage{0, 0});
     *
     * Inject per-frame UI via a callback or by subclassing and overriding BuildUI():
     *
     *     GetPipelinePass<ImGuiPipelinePass>()->SetDrawCallback([](){
     *         ImGui::ShowDemoWindow();
     *     });
     */
    class ImGuiPipelinePass : public PipelinePass
    {
    public:
        using DrawCallback = std::function<void()>;

        explicit ImGuiPipelinePass(Pipeline::Stage stage);
        ~ImGuiPipelinePass() override;

        ImGuiPipelinePass(const ImGuiPipelinePass &) = delete;
        ImGuiPipelinePass &operator=(const ImGuiPipelinePass &) = delete;

        void Render(const CommandBuffer &commandBuffer) override;

        void SetDrawCallback(DrawCallback cb) { drawCallback_ = std::move(cb); }

    protected:
        /// Override to build ImGui windows without using a callback.
        virtual void BuildUI();

    private:
        void Init();
        void Shutdown();
        void CreateDescriptorPool();

        VkDescriptorPool imguiPool_ = VK_NULL_HANDLE;
        bool initialized_ = false;
        DrawCallback drawCallback_;
    };
}
