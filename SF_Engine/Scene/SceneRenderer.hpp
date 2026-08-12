#pragma once
#include <Rendering/Renderer.hpp>
#include <Rendering/Stage.hpp>
#include <Rendering/Lighting/ClusterCullPipelinePass.hpp>
#include <Rendering/Lighting/LitMeshPipelinePass.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Stage.hpp>
#include <Rendering/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>
#include <Rendering/Visuals/sfSkies/AtmosphereController.hpp>

#include <Rendering/PipelinePassInit.hpp>
#include <Rendering/RenderPass/FullscreenPass.hpp>

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
            clusterCull_ = AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);
            litPass_ = AddPipelinePass<LitMeshPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);

            atmoController = std::make_unique<AtmosphereController>(
                Pipeline::Stage{1, 0},
                [this](Pipeline::Stage s, const AtmosphereParams &p)
                { return AddPipelinePass<AtmospherePipelinePass>(s, p); });

            if (config_.enableAtmosphere)
            {
                Vec3 earthPos = {0.0f, -config_.atmosphereParams.bottomRadius, 0.0f};
                // TODO: load from xml
                atmoController->AddAtmosphere("Earth", earthData, earthPos);

                cloudPass_ = AddPipelinePass<CloudPipelinePass>(Pipeline::Stage{1, 0}, earthData);

                // disable cloud pass cuz its broken
                cloudPass_->SetEnabled(false);
                // disable cloud pass cuz its broken

                AddPipelinePass<FullscreenPass>(Pipeline::Stage{1, 0}, "hdr", "Shaders/FullscreenPass.shader");
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
        ClusterCullPipelinePass *GetClusterCull() { return clusterCull_; } // add

        std::unique_ptr<AtmosphereController> atmoController;

    private:
        SceneRendererConfig config_;
        AtmosphereData earthData{config_.atmosphereParams, {}};

        std::unique_ptr<LightManager> lightManager_;
        LitMeshPipelinePass *litPass_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;
        ClusterCullPipelinePass *clusterCull_ = nullptr;

        bool uiCallbackSet_ = false;
        uint32_t lastScreenH_ = 600Ui32, lastScreenW_ = 800Ui32;
    };
}