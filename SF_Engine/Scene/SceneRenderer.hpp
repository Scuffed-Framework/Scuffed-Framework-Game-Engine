#pragma once
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Lighting/ClusterCullPipelinePass.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/AtmospherePipelinePass.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>

#include <Graphics/PipelinePassInit.hpp>
#include <Graphics/RenderPass/FullscreenPass.hpp>

namespace SF::Engine
{
    struct SceneRendererConfig
    {
        bool enableAtmosphere = false;
        AtmosphereParams atmosphereParams = []
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
        /*
            explicit SceneRenderer(SceneRendererConfig cfg = {})
                : config_(std::move(cfg))
            {
                AddRenderStage(std::make_unique<RenderStage>(
                    std::vector<Attachment>{
                        Attachment{0, "depth", Attachment::Type::Depth},
                        Attachment{1, "swapchain", Attachment::Type::Swapchain},
                    },
                    std::vector<SubpassType>{
                        SubpassType{0, {0, 1}},
                    }));
            }*/

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

            if (config_.enableAtmosphere)
            {
                // Stage 1: Compositor. Safely samples finished scene textures and draws to swapchain
                atmoPass_ = AddPipelinePass<AtmospherePipelinePass>(
                    Pipeline::Stage{1, 0}, config_.atmosphereParams);

                cloudPass_ = AddPipelinePass<CloudPipelinePass>(
                    Pipeline::Stage{1, 0}, config_.atmosphereParams);
            }

            GetPipelinePassManager()->RunInitCallbacks();
        }

        void Update() override {} // Heavy per-frame work is driven by Scene::Render()

        Image2d *GetHdrColorTarget();

        // Accessors
        LightManager *GetLightManager() { return lightManager_.get(); }
        LitMeshPipelinePass *GetLitPass() { return litPass_; }
        AtmospherePipelinePass *GetAtmoPass() { return atmoPass_; }
        CloudPipelinePass *GetCloudPass() { return cloudPass_; }

    private:
        SceneRendererConfig config_;

        std::unique_ptr<LightManager> lightManager_;
        LitMeshPipelinePass *litPass_ = nullptr;
        AtmospherePipelinePass *atmoPass_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;
    };
}
