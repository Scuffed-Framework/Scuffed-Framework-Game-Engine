#pragma once
#include <Rendering/Renderer.hpp>
#include <Rendering/Stage.hpp>
#include <Rendering/Lighting/Lighting.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>
#include <Rendering/Visuals/sfSkies/AtmosphereController.hpp>
#include <Rendering/Visuals/SSR/SSRPipelinePass.hpp>

#include <Rendering/PipelinePassInit.hpp>
#include <Rendering/RenderPass/FullscreenPass.hpp>
#include <Rendering/Mesh/MeshFactory.hpp>

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
            // Stage 0 : GBuffer (off-screen MRT, no swapchain). Replaces the
            // old forward "gbuf_depth"+"hdr" pair — SSR (and any future
            // deferred-only effect) needs normals/roughness/metallic per
            // pixel, which a forward-lit-straight-to-hdr pass can't provide.
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

            // Stage 1 : Lighting (+ sky/clouds) → SSR composite → Transparent
            // → Tonemap. SSR gets its own dedicated subpass (1) between
            // deferred-light and forward-transparent — it needs to be a real
            // subpass, not folded into another one, because its Render() is
            // a genuine draw into "hdr" (see SSRPipelinePass's class comment
            // for why the old compute-write-in-PreRender approach silently
            // did nothing: every attachment here uses loadOp=CLEAR, which
            // wipes any PreRender write the instant this renderpass starts).
            AddRenderStage(std::make_unique<RenderStage>(
                std::vector<Attachment>{
                    Attachment{0, "hdr", Attachment::Type::Image,
                               false, VK_FORMAT_R16G16B16A16_SFLOAT,
                               Color{0.0f, 0.0f, 0.0f, 1.0f}},
                    Attachment{1, "swapchain", Attachment::Type::Swapchain},
                },
                std::vector<SubpassType>{
                    SubpassType{0, {0}}, // deferred lighting (+ atmosphere/clouds) → hdr
                    SubpassType{1, {0}}, // SSR composite (additive blend) → hdr
                    SubpassType{2, {0}}, // forward transparent → hdr
                    SubpassType{3, {1}}, // tonemap → swapchain
                }));
        }

        void Start() override
        {
            lightManager_ = std::make_unique<LightManager>();

            // Stage 0 : GBuffer geometry pass — writes gbuf_depth/albedo/normal/pbr.
            clusterCull_ = AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);
            gbuffer_ = AddPipelinePass<GBufferPass>(Pipeline::Stage{0, 0}, *lightManager_);

            // Stage 1, subpass 0 : Deferred lighting resolve → hdr.
            AddPipelinePass<DeferredLightPipelinePass>(Pipeline::Stage{1, 0}, *lightManager_);

            // Stage 1, subpass 1 : Probed Stochastic SSR. PreRender() runs
            // the RayGen/Trace/TemporalAccumulate/SpatialFilter compute
            // stages (against this frame's fresh GBuffer + last frame's
            // resolved hdr); Render() is a real subpass draw that additively
            // blends the result into this frame's hdr, after deferred
            // lighting (subpass 0) and before forward-transparent.
            ssr_ = AddPipelinePass<SSRPipelinePass>(Pipeline::Stage{1, 1}, *lightManager_);

            // Stage 1, subpass 2 : Transparent forward pass.
            AddPipelinePass<ForwardTransparentPipelinePass>(Pipeline::Stage{1, 2}, *lightManager_);

            // Stage 1, subpass 3 : Tonemap hdr → swapchain (unconditional —
            // previously this only ran when atmosphere was enabled, leaving
            // nothing writing to swapchain otherwise).
            AddPipelinePass<FullscreenPass>(
                Pipeline::Stage{1, 3}, "hdr", "Shaders/CompositeSampler.shader");

            // Atmosphere/clouds draw directly into "hdr" as part of stage 1
            // subpass 0, alongside DeferredLightPipelinePass — registration
            // order below places them after it in the same subpass, and
            // PipelinePassManager preserves per-stage insertion order, so
            // they correctly draw sky/cloud on top of (behind, depth-tested)
            // the lit scene rather than needing a separate hdr->hdr blit.
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
        GBufferPass *GetGBuffer() { return gbuffer_; }
        SSRPipelinePass *GetSSR() { return ssr_; }
        CloudPipelinePass *GetCloudPass() { return cloudPass_; }
        AtmosphereController *GetAtmosphereController() { return atmoController.get(); }
        ClusterCullPipelinePass *GetClusterCull() { return clusterCull_; }

        std::unique_ptr<AtmosphereController> atmoController;

    private:
        SceneRendererConfig config_;
        AtmosphereData earthData{config_.atmosphereParams, {}};

        std::unique_ptr<LightManager> lightManager_;
        GBufferPass *gbuffer_ = nullptr;
        SSRPipelinePass *ssr_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;
        ClusterCullPipelinePass *clusterCull_ = nullptr;

        bool uiCallbackSet_ = false;
        uint32_t lastScreenH_ = 600, lastScreenW_ = 800;
    };
}
