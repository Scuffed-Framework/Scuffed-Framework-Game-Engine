// SF_Engine/Scene/SceneRenderer.hpp
#pragma once
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Lighting/ClusterCullPipelinePass.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp>
#include <Graphics/Visuals/Atmosphere/AtmospherePipelinePass.hpp>
#include <Graphics/Visuals/Sun/SunPipelinePass.hpp>
#include <Graphics/Visuals/Clouds/CloudPipelinePass.hpp>
#include <ImGui/ImGuiPipelinePass.hpp>
#include <Graphics/Mesh/Mesh.hpp>

namespace SF::Engine
{
    struct SceneRendererConfig
    {
        bool enableAtmosphere = false;
        bool enableSun = false;
        bool enableClouds = false;

        // Atmosphere params  only used if enableAtmosphere = true
        AtmosphereParams atmosphereParams = []
        {
            AtmosphereParams ap;
            ap.bottomRadius = 6371000.0f;
            ap.topRadius = 6471000.0f;
            ap.sunIntensity = 40.0f;
            ap.renderUnitRadius = 6371000.0f;
            return ap;
        }();

        // Sun params  only used if enableSun = true
        SunParams sunParams = []
        {
            SunParams sp;
            sp.intensity = 20.0f;
            sp.discAngleDeg = 0.8f;
            sp.haloAngleDeg = 5.0f;
            sp.haloStrength = 0.30f;
            sp.bloomStrength = 6.0f;
            return sp;
        }();
    };

    class SceneRenderer : public Renderer
    {
    public:
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
        }

        void Start() override
        {
            lightManager_ = std::make_unique<LightManager>();

            // Cluster cull runs in pre-render (before the renderpass)
            AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);

            // Forward lit opaque
            litPass_ = AddPipelinePass<LitMeshPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);

            // Optional atmosphere
            if (config_.enableAtmosphere)
                atmoPass_ = AddPipelinePass<AtmospherePipelinePass>(
                    Pipeline::Stage{0, 0}, config_.atmosphereParams);

            // Optional sun disc
            if (config_.enableSun)
                sunPass_ = AddPipelinePass<SunPipelinePass>(
                    Pipeline::Stage{0, 0}, config_.sunParams);

            // Optional volumetric clouds  must be registered before ImGui
            // so that ImGui always composites on top.
            if (config_.enableClouds)
                cloudPass_ = AddPipelinePass<CloudPipelinePass>(Pipeline::Stage{0, 0});

            // ImGui  always last so it composites on top of everything
            imguiPass_ = AddPipelinePass<ImGuiPipelinePass>(Pipeline::Stage{0, 0});
        }

        void Update() override {} // Heavy per-frame work is driven by Scene::Render()

        // Accessors
        LightManager *GetLightManager() { return lightManager_.get(); }
        LitMeshPipelinePass *GetLitPass() { return litPass_; }
        AtmospherePipelinePass *GetAtmoPass() { return atmoPass_; }
        SunPipelinePass *GetSunPass() { return sunPass_; }
        CloudPipelinePass *GetCloudPass() { return cloudPass_; }

    private:
        SceneRendererConfig config_;

        std::unique_ptr<LightManager> lightManager_;
        LitMeshPipelinePass *litPass_ = nullptr;
        AtmospherePipelinePass *atmoPass_ = nullptr;
        SunPipelinePass *sunPass_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;
        ImGuiPipelinePass *imguiPass_ = nullptr;
    };
}