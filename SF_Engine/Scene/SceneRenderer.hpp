#pragma once
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Lighting/ClusterCullPipelinePass.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>
#include <Graphics/Visuals/sfSkies/AtmosphereController.hpp>

#include <Graphics/PipelinePassInit.hpp>
#include <Graphics/RenderPass/FullscreenPass.hpp>

namespace SF::Engine
{
    class Scene;

    struct SceneRendererConfig
    {
        bool enableAtmosphere = false; 
        AtmosphereParams atmosphereParams /*Earth*/ = []
        {
            AtmosphereParams ap;
            ap.bottomRadius = 6371000.0f;
            ap.topRadius = 6471000.0f;
            ap.sunIntensity = 40.0f;
            ap.renderUnitRadius = 6371000.0f;
            return ap;
        }();
    };

    class SceneRenderer : public Renderer
    {
    public:
        SceneRenderer(SceneRendererConfig cfg = {}) : config_(std::move(cfg))
        {
            // Stage 0: opaque forward pass — depth + offscreen HDR color
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "gbuf_depth", Attachment::Type::Depth},
                    Attachment{1, "hdr", Attachment::Type::Image,
                               false, VK_FORMAT_R16G16B16A16_SFLOAT,
                               Color{0.0f, 0.0f, 0.0f, 1.0f}},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0, 1}},
                }));

            // Stage 1: composite (atmosphere+clouds, or plain blit) → swapchain
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "swapchain", Attachment::Type::Swapchain},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0}},
                }));
        }

        void Start() override
        {
            lightManager_ = std::make_unique<LightManager>();

            // Stage 0: Opaque geometry safely renders into scene_color + gbuf_depth (scene_color is hdr)
            AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);
            litPass_ = AddPipelinePass<LitMeshPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);

            atmoController = std::make_unique<AtmosphereController>(
            Pipeline::Stage{1, 0},
            [this](Pipeline::Stage s, const AtmosphereParams &p) { return AddPipelinePass<AtmospherePipelinePass>(s, p); });

            if (config_.enableAtmosphere)
            {
                AtmosphereData earthData{ config_.atmosphereParams, {} };
                Vec3 earthPos = {0.0f, -config_.atmosphereParams.bottomRadius, 0.0f};
                // TODO: load from xml
                atmoController->AddAtmosphere("Earth", earthData, earthPos);

                cloudPass_ = AddPipelinePass<CloudPipelinePass>(Pipeline::Stage{1, 0}, config_.atmosphereParams);
            }

            GetPipelinePassManager()->RunInitCallbacks();
        }

        void Update() override {} // Heavy per-frame work is driven by RenderScene()

        // Renders one frame for the given scene: camera update, lighting upload,
        // mesh submission, atmosphere/cloud frame data. Replaces Scene::Render().
        void RenderScene(Scene *scene);

        Image2d *GetHdrColorTarget();

        // Accessors
        LightManager *GetLightManager() { return lightManager_.get(); }
        LitMeshPipelinePass *GetLitPass() { return litPass_; }
        CloudPipelinePass *GetCloudPass() { return cloudPass_; }
        AtmosphereController *GetAtmosphereController() { return atmoController.get(); }

        std::unique_ptr<AtmosphereController> atmoController;

    private:
        SceneRendererConfig config_;

        std::unique_ptr<LightManager> lightManager_;
        LitMeshPipelinePass *litPass_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;

        bool uiCallbackSet_ = false;
    };
}