#pragma once
#include <Gui/ImGui/ocornut/imgui_impl_vulkan.h>
#include <Rendering/Camera/Camera.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <Rendering/RenderSystem.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SF::Engine
{
    class SceneViewport : NoCopy
    {
    public:
        explicit SceneViewport(UVec2 extent = {1280, 720});
        ~SceneViewport();

        void SetDesiredExtent(UVec2 extent);
        void Tick(std::size_t frameIndex);
        void PrepareForRender(VkCommandBuffer cmd);
        void BeginRendering(VkCommandBuffer cmd, VkClearColorValue clear = {{0.02f, 0.02f, 0.02f, 1.0f}});
        void EndRendering(VkCommandBuffer cmd);
        void PrepareForSample(VkCommandBuffer cmd);

        Camera &GetCamera() noexcept { return camera; }
        [[nodiscard]] VkDescriptorSet GetImGuiTexture() const noexcept { return imguiDescriptor; }
        [[nodiscard]] UVec2 GetExtent() const noexcept { return currentExtent; }
        [[nodiscard]] VkDescriptorSet GetImGuiTexture();
        [[nodiscard]] bool IsVisible() const noexcept { return visible; }
        void SetVisible(bool v) noexcept { visible = v; }

    private:
        void CreateImages(UVec2 extent);

        Camera camera;
        std::unique_ptr<Image2d> color;
        std::unique_ptr<ImageDepth> depth;
        VkDescriptorSet imguiDescriptor = VK_NULL_HANDLE;
        std::vector<std::optional<VkDescriptorSet>> pendingFree{};
        // Images retired by a viewport resize. They're referenced by command buffers still in
        // flight (as the render target, and as the ImGui texture), so they can't be destroyed the
        // moment Tick() replaces them -- they're parked per frame slot and only freed when that slot
        // comes round again (>= kFramesInFlight frames later), mirroring pendingFree above.
        struct RetiredImages
        {
            std::unique_ptr<Image2d> color;
            std::unique_ptr<ImageDepth> depth;
        };
        std::vector<RetiredImages> retired{};
        UVec2 currentExtent{};
        UVec2 desiredExtent{};
        bool resizePending                           = false;
        VkImageLayout currentLayout                  = VK_IMAGE_LAYOUT_UNDEFINED;
        bool visible                                 = true;
        static constexpr std::size_t kFramesInFlight = 3;
    };

    inline void DrawViewport(SceneViewport *vp)
    {
        ImGui::Begin("Scene Viewport");

        const UVec2 avail{static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                          static_cast<uint32_t>(ImGui::GetContentRegionAvail().y)};

        vp->SetDesiredExtent(avail);

        // awful. just awful.
        ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(vp->GetImGuiTexture())),
                     ImGui::GetContentRegionAvail());

        ImGui::End();
    }
} // namespace SF::Engine
