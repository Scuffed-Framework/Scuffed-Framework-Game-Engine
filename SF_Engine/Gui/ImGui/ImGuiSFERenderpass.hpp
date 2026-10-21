#pragma once
#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <volk.h>

#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Commands/CommandBuffer.hpp>

// Always use the GLFW backend : it auto-installs all input callbacks and works
// on every platform (including Windows). The Win32 backend is only needed when
// GLFW is not in use.
#include <Gui/ImGui/ocornut/imgui.h>
#include <Gui/ImGui/ocornut/imgui_impl_glfw.h>
#include <Gui/ImGui/ocornut/imgui_impl_vulkan.h>

#include <functional>

namespace SF::Engine
{
    /**
     * @brief SFERenderpass that integrates Dear ImGui into the SF render pipeline.
     *
     * Setup:
     * Add this SFERenderpass LAST within a subpass so it composites on top:
     *
     *     AddSFERenderpass<ImGuiSFERenderpass>(Pipeline::Stage{0, 0});
     *
     * Inject per-frame UI via a callback or by subclassing and overriding BuildUI():
     *
     *     GetSFERenderpass<ImGuiSFERenderpass>()->SetDrawCallback([](){
     *         ImGui::ShowDemoWindow();
     *     });
     */
    class ImGuiSFERenderpass : public EngineRenderpass
    {
    public:
        static void SetTargetStage(Pipeline::Stage stage) { s_targetStage = stage; }

    private:
        inline static Pipeline::Stage s_targetStage{0, 0}; // default preserves old behavior

        inline static bool s_registered = []()
        {
            EngineRenderpassInitRegistry::Get().Register(
                    [](EngineRenderpassManager &mgr)
                    {
                        mgr.Add<ImGuiSFERenderpass>(s_targetStage, std::make_unique<ImGuiSFERenderpass>(s_targetStage));
                    });
            return true;
        }();

    public:
        using DrawCallback = std::function<void()>;

        explicit ImGuiSFERenderpass(Pipeline::Stage stage);
        ~ImGuiSFERenderpass() override;

        ImGuiSFERenderpass(const ImGuiSFERenderpass &)            = delete;
        ImGuiSFERenderpass &operator=(const ImGuiSFERenderpass &) = delete;

        void Render(const CommandBuffer &commandBuffer) override;

        void SetDrawCallback(DrawCallback cb) { drawCallback_ = std::move(cb); }

    protected:
        /// Override to build ImGui windows without using a callback.
        virtual void BuildUI();

    private:
        inline static bool s_backendInitialized = false;

        void Init();
        void Shutdown();
        void CreateDescriptorPool();

        VkDescriptorPool imguiPool_ = VK_NULL_HANDLE;
        bool initialized_           = false;
        DrawCallback drawCallback_;
    };
} // namespace SF::Engine
