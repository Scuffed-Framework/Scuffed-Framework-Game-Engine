#pragma once
#include <Rendering/FrameGraph/Stage.hpp>
#include <Rendering/Lighting/Lighting.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/RHI/Images/Image2d.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Visuals/SSR/SSRPipelinePass.hpp>
#include <Rendering/Visuals/sfSkies/AtmosphereController.hpp>
#include <Rendering/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>

#include <Rendering/FrameGraph/EngineRenderpassInitRegistry.hpp>
#include <Rendering/Mesh/MeshFactory.hpp>
#include <Rendering/RHI/Renderpass/FullscreenPass.hpp>

namespace SF::Engine
{
    class Scene;

    struct SceneRendererConfig
    {
        bool enableAtmosphere                       = false;
        AtmosphereParams atmosphereParams /*Earth*/ = []
        {
            AtmosphereParams ap;
            ap.bottomRadius     = 6371000.0f;
            ap.topRadius        = 6471000.0f;
            ap.sunIntensity     = 40.0f;
            ap.renderUnitRadius = 6371000.0f;
            return ap;
        }();
    };

    class SceneRenderer : public Renderer
    {
    public:
        explicit SceneRenderer(const SceneRendererConfig &cfg = {}) : config_(cfg)
        {
            // todo: replace this shit with a render graph
            AddRenderStage(std::make_unique<RhiRenderStage>(
                    std::vector<RhiAttachment>{
                            RhiAttachment{0, "gbuf_depth", RhiAttachment::Type::Depth},
                            RhiAttachment{1, "gbuf_albedo", RhiAttachment::Type::Image, false,
                                          VK_FORMAT_R8G8B8A8_UNORM},
                            RhiAttachment{2, "gbuf_normal", RhiAttachment::Type::Image, false, VK_FORMAT_R16G16_SNORM},
                            RhiAttachment{3, "gbuf_pbr", RhiAttachment::Type::Image, false, VK_FORMAT_R8G8B8A8_UNORM},
                    },
                    std::vector<RhiSubpassType>{
                            RhiSubpassType{0, {0, 1, 2, 3}},
                    }));

            AddRenderStage(std::make_unique<RhiRenderStage>(
                    std::vector<RhiAttachment>{
                            RhiAttachment{0, "hdr", RhiAttachment::Type::Image, false, VK_FORMAT_R16G16B16A16_SFLOAT,
                                          Color{0.0f, 0.0f, 0.0f, 1.0f}},
                    },
                    std::vector<RhiSubpassType>{
                            RhiSubpassType{0, {0}}, // deferred lighting (+ atmosphere/clouds) → hdr
                            RhiSubpassType{1, {0}}, // SSR composite (additive blend) → hdr
                            RhiSubpassType{2, {0}}, // forward transparent → hdr
                    }));

            AddRenderStage(std::make_unique<RhiRenderStage>(
                    std::vector<RhiAttachment>{
                            RhiAttachment{0, "swapchain", RhiAttachment::Type::Swapchain},
                    },
                    std::vector<RhiSubpassType>{
                            RhiSubpassType{0, {0}},
                    }));
        }

        void Start() override;

        void Update() override {} // Heavy per-frame work is driven by RenderScene()

        // Renders one frame for the given scene: camera update, lighting upload,
        // mesh submission, atmosphere/cloud frame data. Replaces Scene::Render().
        void RenderScene(Scene *scene);

        Image2d *GetHdrColorTarget();

        // Accessors
        LightManager *GetLightManager() const { return lightManager_.get(); }
        GBufferPass *GetGBuffer() const { return gbuffer_; }
        SSRPipelinePass *GetSSR() const { return ssr_; }
        CloudPipelinePass *GetCloudPass() const { return cloudPass_; }
        AtmosphereController *GetAtmosphereController() const { return atmoController.get(); }
        ClusterCullPipelinePass *GetClusterCull() const { return clusterCull_; }

        std::unique_ptr<AtmosphereController> atmoController;

    private:
        SceneRendererConfig config_;
        AtmosphereData earthData{config_.atmosphereParams, {}};

        std::unique_ptr<LightManager> lightManager_;
        GBufferPass *gbuffer_                 = nullptr;
        SSRPipelinePass *ssr_                 = nullptr;
        CloudPipelinePass *cloudPass_         = nullptr;
        ClusterCullPipelinePass *clusterCull_ = nullptr;

        bool uiCallbackSet_   = false;
        uint32_t lastScreenH_ = 600, lastScreenW_ = 800;
    };
} // namespace SF::Engine
