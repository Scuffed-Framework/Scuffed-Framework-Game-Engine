#pragma once

#include <Rendering/Renderer.hpp>
#include <Rendering/Stage.hpp>
#include <Rendering/RenderPass/FullscreenPass.hpp>
#include <Rendering/Lighting/Lighting.hpp>
#include <Rendering/Mesh/MeshFactory.hpp>
#include "Windows/WindowManager.hpp"

#include <Math/BasicMath.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace SF::Engine
{
    class ForwardLitRenderer : public SF::Engine::Renderer
    {
    public:
        ForwardLitRenderer()
        {
            using namespace SF::Engine;
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "depth", Attachment::Type::Depth},
                    Attachment{1, "swapchain", Attachment::Type::Swapchain},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0, 1}},
                }));
        }

        void Start() override
        {
            lightManager_ = std::make_unique<LightManager>();

            // Compute cluster cull runs in PreRender (before renderpass)
            clusterCull_ = AddPipelinePass<ClusterCullPipelinePass>(
                Pipeline::Stage{0, 0}, *lightManager_);

            // Forward lit pass
            litPass_ = AddPipelinePass<LitMeshPipelinePass>(
                Pipeline::Stage{0, 0}, *lightManager_);

            // Default lights
            Light sun{};
            sun.type = Lighting::LightType::Directional;
            sun.direction = normalize(Vec3(-0.5f, -1.0f, -0.3f));
            sun.color = {1.0f, 0.95f, 0.85f};
            sun.intensity = 3.0f;
            lightManager_->AddLight(sun);

            Light blue{};
            blue.type = Lighting::LightType::Point;
            blue.position = {4.0f, 3.0f, 4.0f};
            blue.color = {0.3f, 0.6f, 1.0f};
            blue.intensity = 20.0f;
            blue.radius = 15.0f;
            lightManager_->AddLight(blue);

            Light orange{};
            orange.type = Lighting::LightType::Point;
            orange.position = {-4.0f, 2.0f, -2.0f};
            orange.color = {1.0f, 0.4f, 0.2f};
            orange.intensity = 15.0f;
            orange.radius = 12.0f;
            lightManager_->AddLight(orange);

            // Demo mesh
            demoMesh_ = SF::Engine::MeshFactory::CreateSphere();
        }

        void Update() override
        {
            // litPass_->Submit();
        }

        SF::Engine::LightManager *GetLightManager() { return lightManager_.get(); }
        SF::Engine::LitMeshPipelinePass *GetLitPass() { return litPass_; }

    private:
        std::unique_ptr<SF::Engine::LightManager> lightManager_;
        SF::Engine::ClusterCullPipelinePass *clusterCull_ = nullptr;
        SF::Engine::LitMeshPipelinePass *litPass_ = nullptr;
        std::unique_ptr<SF::Engine::Mesh> demoMesh_;
    };

    class LightingRenderer : public SF::Engine::Renderer
    {
    public:
        LightingRenderer()
        {
            using namespace SF::Engine;

            // Stage 0: GBuffer (off-screen MRT, no swapchain)
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "gbuf_depth", Attachment::Type::Depth},
                    Attachment{1, "gbuf_albedo", Attachment::Type::Image,
                               false, VK_FORMAT_R8G8B8A8_UNORM},
                    Attachment{2, "gbuf_normal", Attachment::Type::Image,
                               false, VK_FORMAT_R16G16_SNORM},
                    Attachment{3, "gbuf_pbr", Attachment::Type::Image,
                               false, VK_FORMAT_R8G8B8A8_UNORM},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0, 1, 2, 3}},
                }));

            // Stage 1: Lighting + Transparent + Tonemap
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "hdr", Attachment::Type::Image,
                               false, VK_FORMAT_R16G16B16A16_SFLOAT,
                               Color{0.0f, 0.0f, 0.0f, 1.0f}},
                    Attachment{1, "swapchain", Attachment::Type::Swapchain},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0}}, // deferred lighting → hdr
                    SubpassType{1, {0}}, // forward transparent → hdr
                    SubpassType{2, {1}}, // tonemap → swapchain
                }));
        }

        void Start() override
        {
            lightManager_ = std::make_unique<LightManager>();

            // Stage 0, subpass 0 : GBuffer geometry
            // ClusterCull runs in PreRender for stage 0 (before GBuffer renderpass)
            AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);
            gbuffer_ = AddPipelinePass<GBufferPass>(Pipeline::Stage{0, 0}, *lightManager_);

            // Stage 1, subpass 0 : Deferred lighting resolve
            AddPipelinePass<DeferredLightPipelinePass>(Pipeline::Stage{1, 0}, *lightManager_);

            // Stage 1, subpass 1 : Transparent forward pass
            AddPipelinePass<ForwardTransparentPipelinePass>(Pipeline::Stage{1, 1}, *lightManager_);

            // Stage 1, subpass 2 : Tonemap hdr → swapchain
            AddPipelinePass<FullscreenPass>(
                Pipeline::Stage{1, 2}, "hdr", "Shaders/FullscreenPass.shader");

            // Default lights
            Light sun{};
            sun.type = Lighting::LightType::Directional;
            sun.direction = normalize(Vec3(-0.5f, -1.0f, -0.3f));
            sun.color = {1.0f, 0.95f, 0.85f};
            sun.intensity = 3.0f;
            lightManager_->AddLight(sun);

            Light fill{};
            fill.type = Lighting::LightType::Point;
            fill.position = {4.0f, 3.0f, 4.0f};
            fill.color = {0.3f, 0.6f, 1.0f};
            fill.intensity = 20.0f;
            fill.radius = 15.0f;
            lightManager_->AddLight(fill);
        }

        void Update() override
        {
        }

        Image2d *GetHdrColorTarget();

        SF::Engine::LightManager *GetLightManager() { return lightManager_.get(); }
        SF::Engine::GBufferPass *GetGBuffer() { return gbuffer_; }

    private:
        std::unique_ptr<SF::Engine::LightManager> lightManager_;
        SF::Engine::GBufferPass *gbuffer_ = nullptr;
    };
}
