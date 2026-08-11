#include "ImGuiPipelinePass.hpp"

#include <Rendering/RenderSystem.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/RenderPass/RenderPass.hpp>
#include <Rendering/Stage.hpp>

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <Default/ImGuiDefault.hpp>

namespace SF::Engine
{
    ImGuiPipelinePass::ImGuiPipelinePass(Pipeline::Stage stage)
        : PipelinePass(stage)
    {
        Init();
    }

    ImGuiPipelinePass::~ImGuiPipelinePass()
    {
        Shutdown();
    }

    void ImGuiPipelinePass::CreateDescriptorPool()
    {
        auto *logDevice = RenderSystem::Get()->GetLogicalDevice();

        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        RenderSystem::CheckVkResult(
            vkCreateDescriptorPool(*logDevice, &poolInfo, nullptr, &imguiPool_));
    }

    void ImGuiPipelinePass::Init()
    {
        if (s_backendInitialized)
        {
            // Another instance already owns the context; this instance is a duplicate and should not re-initialize anything.
            // Log an error because this isn't supposed to happen
            Log::Error("[ImGui Pass Backend] ImGui Backend tried initializing, but was previously initialized!");
            return;
        }

        auto *renderSystem = RenderSystem::Get();
        auto *physDevice = renderSystem->GetPhysicalDevice();
        auto *logDevice = renderSystem->GetLogicalDevice();
        auto *window = WindowManager::Get()->GetWindow(0);

        if (!window)
            throw std::runtime_error("ImGuiPipelinePass: no window available");

        GLFWwindow *glfwWindow = window->GetWindow();
        if (!glfwWindow)
            throw std::runtime_error("ImGuiPipelinePass: null GLFWwindow");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(glfwWindow, /*install_callbacks=*/false))
            throw std::runtime_error("ImGuiPipelinePass: ImGui_ImplGlfw_InitForVulkan failed");

        CreateDescriptorPool();

        const auto *renderStage = renderSystem->GetRenderStage(GetStage().first);
        if (!renderStage || !renderStage->GetRenderpass())
            throw std::runtime_error("ImGuiPipelinePass: render stage / renderpass not ready");

        const VkRenderPass vkRenderPass = renderStage->GetRenderpass()->GetRenderpass();

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_2;
        initInfo.Instance = renderSystem->GetInstance()->GetInstance();
        initInfo.PhysicalDevice = physDevice->GetPhysicalDevice();
        initInfo.Device = logDevice->GetLogicalDevice();
        initInfo.QueueFamily = logDevice->GetGraphicsFamily();
        initInfo.Queue = logDevice->GetGraphicsQueue();
        initInfo.PipelineCache = renderSystem->GetPipelineCache();
        initInfo.DescriptorPool = imguiPool_;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = renderSystem->GetSwapchain(0)->GetImageCount();
        initInfo.Allocator = nullptr;
        initInfo.CheckVkResultFn = [](VkResult r)
        { RenderSystem::CheckVkResult(r); };

        initInfo.PipelineInfoMain.RenderPass = vkRenderPass;
        initInfo.PipelineInfoMain.Subpass = GetStage().second;
        // ImGui renders into the swapchain subpass which is always 1x (non-multisampled).
        // Passing physDevice->GetMsaaSamples() here would cause a spec violation because
        // the swapchain attachment sample count is 1x regardless of device MSAA capability.
        initInfo.PipelineInfoMain.MSAASamples = renderStage->IsMultisampled(GetStage().second)
                                                    ? physDevice->GetMsaaSamples()
                                                    : VK_SAMPLE_COUNT_1_BIT;

        VkInstance vulkan_instance = RenderSystem::Get()->GetInstance()->GetInstance();

        // 2. Pass the handle itself casted to void*, rather than a pointer to the handle
        ImGui_ImplVulkan_LoadFunctions(RenderSystem::Get()->GetVkAPIVersion(), [](const char *function_name, void *user_data)
                                       {
                                           // Cast the void* directly back to a VkInstance
                                           return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), function_name);
                                       },
                                       reinterpret_cast<void *>(vulkan_instance));

        if (!ImGui_ImplVulkan_Init(&initInfo))
            throw std::runtime_error("ImGuiPipelinePass: ImGui_ImplVulkan_Init failed");

        initialized_ = true;
        s_backendInitialized = true;

        ImGuiDefaultStyle::SetStyle();
    }

    void ImGuiPipelinePass::Shutdown()
    {
        if (!initialized_)
            return;

        vkDeviceWaitIdle(*RenderSystem::Get()->GetLogicalDevice());

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (imguiPool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(
                *RenderSystem::Get()->GetLogicalDevice(), imguiPool_, nullptr);
            imguiPool_ = VK_NULL_HANDLE;
        }

        initialized_ = false;
        s_backendInitialized = false;
    }

    void ImGuiPipelinePass::BuildUI()
    {
        if (drawCallback_)
            drawCallback_();
    }

    void ImGuiPipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        if (!initialized_)
            return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0u, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);
        BuildUI();
        
        ImGui::Render();
        ImDrawData *drawData = ImGui::GetDrawData();
        if (drawData && drawData->TotalVtxCount > 0)
            ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
}
