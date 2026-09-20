#include "SceneRenderer.hpp"
#include <Gui/ImGuiPipelinePass.hpp>
#include <Gui/UIRegistry.hpp>
#include <Math/Transform.hpp>
#include <Platform/Windowing/WindowManager.hpp>
#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Images/ImageDepth.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Scene/Scene.hpp>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

namespace SF::Engine
{
    namespace
    {
        std::function<Texture()> ResolveColorAttachment(std::string name)
        {
            return [name = std::move(name)]
            {
                auto *img =
                        const_cast<Image2d *>(dynamic_cast<const Image2d *>(RenderSystem::Get()->GetAttachment(name)));
                if (!img)
                    return Texture{};
                return Texture{img->GetImage(), img->GetView(), img->GetFormat(), VK_IMAGE_ASPECT_COLOR_BIT};
            };
        }

        std::function<Texture()> ResolveDepthAttachment(std::string name)
        {
            return [name = std::move(name)]
            {
                auto *img = const_cast<ImageDepth *>(
                        dynamic_cast<const ImageDepth *>(RenderSystem::Get()->GetAttachment(name)));
                if (!img)
                    return Texture{};
                return Texture{img->GetImage(), img->GetView(), img->GetFormat(), VK_IMAGE_ASPECT_DEPTH_BIT};
            };
        }
    } // namespace

    void SceneRenderer::Start()
    {
        lightManager_ = std::make_unique<LightManager>();


        clusterCull_ = AddPipelinePass<ClusterCullPipelinePass>(Pipeline::Stage{0, 0}, *lightManager_);
        gbuffer_     = AddPipelinePass<GBufferPass>(Pipeline::Stage{0, 0}, *lightManager_);

        // Stage 1, subpass 0 : Deferred lighting resolve -> hdr.
        auto *deferredLight = AddPipelinePass<DeferredLightPipelinePass>(Pipeline::Stage{1, 0}, *lightManager_);

        ssr_ = AddPipelinePass<SSRPipelinePass>(Pipeline::Stage{1, 1}, *lightManager_);

        // Stage 1, subpass 2 : Transparent forward pass.
        auto *forwardTransparent =
                AddPipelinePass<ForwardTransparentPipelinePass>(Pipeline::Stage{1, 2}, *lightManager_);

        auto *composite =
                AddPipelinePass<FullscreenPass>(Pipeline::Stage{2, 0}, "hdr", "Shaders/CompositeSampler.shader");

        atmoController = std::make_unique<AtmosphereController>(
                Pipeline::Stage{1, 0}, [this](const Pipeline::Stage &s, const AtmosphereParams &p)
                { return AddPipelinePass<AtmospherePipelinePass>(s, p); });

        if (config_.enableAtmosphere)
        {
            const Vec3 earthPos = {0.0f, -config_.atmosphereParams.bottomRadius, 0.0f};
            // TODO: load from xml
            atmoController->AddAtmosphere("Earth", earthData, earthPos);

            cloudPass_ = AddPipelinePass<CloudPipelinePass>(Pipeline::Stage{1, 0}, earthData);

            // disable cloud pass cuz its broken
            cloudPass_->SetEnabled(false);
            // disable cloud pass cuz its broken
        }

        GetPipelinePassManager()->RunInitCallbacks();

        // Only the passes above that touch image resources genuinely shared across pass
        // boundaries get registered; see the ClusterCullPipelinePass comment above and the
        // per-pass notes below for what's deliberately left out and why.
        auto &graph = GetFrameGraph();

        const auto albedo = graph.ImportResource("gbuf_albedo", ResolveColorAttachment("gbuf_albedo"));
        const auto normal = graph.ImportResource("gbuf_normal", ResolveColorAttachment("gbuf_normal"));
        const auto pbr    = graph.ImportResource("gbuf_pbr", ResolveColorAttachment("gbuf_pbr"));
        const auto depth  = graph.ImportResource("gbuf_depth", ResolveDepthAttachment("gbuf_depth"));
        const auto hdr    = graph.ImportResource("hdr", ResolveColorAttachment("hdr"));

        auto &gbufferNode = graph.AddPass("GBuffer", *gbuffer_);
        graph.Create(gbufferNode, albedo, ResourceUsage::ColorAttachment);
        graph.Create(gbufferNode, normal, ResourceUsage::ColorAttachment);
        graph.Create(gbufferNode, pbr, ResourceUsage::ColorAttachment);
        graph.Create(gbufferNode, depth, ResourceUsage::DepthStencilAttachment);

        auto &deferredLightNode = graph.AddPass("DeferredLight", *deferredLight);
        graph.Read(deferredLightNode, albedo, ResourceUsage::SampledTexture);
        graph.Read(deferredLightNode, normal, ResourceUsage::SampledTexture);
        graph.Read(deferredLightNode, pbr, ResourceUsage::SampledTexture);
        graph.Read(deferredLightNode, depth, ResourceUsage::SampledTexture);

        graph.Create(deferredLightNode, hdr, ResourceUsage::ColorAttachment);

        if (auto *atmosphere = GetPipelinePassManager()->Get<AtmospherePipelinePass>())
        {
            auto &atmosphereNode = graph.AddPass("Atmosphere", *atmosphere);
            graph.Write(atmosphereNode, hdr, ResourceUsage::ColorAttachment);
        }

        auto &ssrNode = graph.AddPass("SSR", *ssr_);
        graph.Write(ssrNode, hdr, ResourceUsage::ColorAttachment);

        auto &forwardTransparentNode = graph.AddPass("ForwardTransparent", *forwardTransparent);
        graph.Write(forwardTransparentNode, hdr, ResourceUsage::ColorAttachment);

        auto &compositeNode = graph.AddPass("Composite", *composite);
        graph.Read(compositeNode, hdr, ResourceUsage::SampledTexture);
        // Writes the swapchain, which isn't modeled as a FrameGraph resource (its underlying
        // VkImage changes identity every frame on acquire). MarkSideEffect keeps this pass,
        // and by extension everything it transitively depends on, alive regardless.
        graph.MarkSideEffect(compositeNode);

        graph.Compile();
    }

    Image2d *SceneRenderer::GetHdrColorTarget()
    {
        auto *rs  = RenderSystem::Get();
        auto *hdr = dynamic_cast<const Image2d *>(rs->GetAttachment("swapchain"));
        return const_cast<Image2d *>(hdr);
    }

    void SceneRenderer::RenderScene(Scene *scene)
    {
        if (!gbuffer_ || !lightManager_ || !scene)
            return;

        if (!uiCallbackSet_)
        {
            uiCallbackSet_ = true;

            if (auto *imgui = GetPipelinePassManager()->Get<ImGuiPipelinePass>())
            {
                imgui->SetDrawCallback([this, scene]() { UIRegistry::Get().DrawAll(); });
            }
        }

        auto now              = std::chrono::steady_clock::now();
        float dt              = std::min(std::chrono::duration<float>(now - scene->lastFrameTime_).count(), 0.1f);
        scene->lastFrameTime_ = now;
        scene->elapsed_ += dt;

        auto *wnd = WindowManager::Get()->GetWindow(0);
        auto &io  = ImGui::GetIO();

        CameraController *cameraController = scene->GetCamera();
        cameraController->SetFrameInput(wnd, io.WantCaptureMouse, io.WantCaptureKeyboard);
        cameraController->Update(dt);
        Camera *cam = cameraController->GetActive();

        float aspect    = wnd ? wnd->GetAspectRatio() : 1.0f;
        Vec2 screenSize = wnd ? Vec2(wnd->GetSize().x, wnd->GetSize().y) : Vec2(800.0f, 600.0f);

        uint32_t curW = static_cast<uint32_t>(screenSize.x);
        uint32_t curH = static_cast<uint32_t>(screenSize.y);
        if ((curW != lastScreenW_ || curH != lastScreenH_) && clusterCull_)
        {
            clusterCull_->MarkDirty();
            lastScreenW_ = curW;
            lastScreenH_ = curH;
        }

        Mat4 view = cam->GetView();
        Mat4 proj = cam->GetProjection(aspect);

        scene->SyncLightTransforms();
        scene->RebuildLightManager();

        Lighting::GpuFrameData fd{};
        fd.view          = view;
        fd.proj          = proj;
        fd.viewProj      = proj * view;
        fd.invView       = inverse(view);
        fd.invProj       = inverse(proj);
        fd.invViewProj   = inverse(fd.viewProj);
        fd.cameraPos     = Vec4(cam->GetPosition(), cam->GetNearPlane());
        fd.cameraDir     = Vec4(cam->GetFront(), cam->GetFarPlane());
        fd.screenSize    = screenSize;
        fd.invScreenSize = 1.0f / screenSize;
        fd.nearPlane     = cam->GetNearPlane();
        fd.farPlane      = cam->GetFarPlane();
        fd.time          = scene->elapsed_;
        fd.deltaTime     = dt;
        fd.lightCount    = lightManager_->GetLightCount();
        fd.frameIndex    = scene->frameIndex_++;

        Vec3 sunDir   = normalize(Vec3(0.0f, 0.0f, 0.0f));
        Vec3 sunColor = Vec3(1.0f, 1.0f, 1.0f);
        float sunInt  = 1.0f;
        if (!scene->lights_.empty() &&
            scene->lights_[0]->GetComponent<Light>()->type == Lighting::LightType::Directional)
        {
            Vec3 ld  = normalize(scene->lights_[0]->GetComponent<Light>()->direction);
            sunDir   = -ld;
            sunColor = scene->lights_[0]->GetComponent<Light>()->color;
            sunInt   = scene->lights_[0]->GetComponent<Light>()->intensity;
        }
        fd.sunDirIntensity = Vec4(sunDir, sunInt);

        lightManager_->Upload(fd);

        // Legacy single-planet centre, still used by CloudPipelinePass until it's
        // converted to per-planet data like AtmosphereController.
        Vec3 planetCentre = {0.0f, -6371000.0f, 0.0f};

        for (auto &obj: scene->objects_)
        {
            if (obj->enabled && obj->mesh)
                gbuffer_->Submit(obj->mesh, *obj->GetComponent<MeshMaterial>(),
                                 obj->GetComponent<Transform>()->ToMatrix());
        }

        if (atmoController && !atmoController->Empty())
        {
            Vec3 atmoSun = sunDir;
            if (!scene->lights_.empty() &&
                scene->lights_[0]->GetComponent<Light>()->type == Lighting::LightType::Directional)
            {
                Vec3 ld = normalize(scene->lights_[0]->GetComponent<Light>()->direction);
                if (glm::length(ld) > 0.5f)
                    atmoSun = -ld;
            }
            atmoController->SetFrameData(inverse(proj), inverse(view), cam->GetPosition(), atmoSun, screenSize);
        }

        if (cloudPass_)
        {
            Vec3 cloudSun = sunDir;
            if (!scene->lights_.empty() &&
                scene->lights_[0]->GetComponent<Light>()->type == Lighting::LightType::Directional)
            {
                Vec3 ld = normalize(scene->lights_[0]->GetComponent<Light>()->direction);
                if (glm::length(ld) > 0.5f)
                    cloudSun = -ld;
            }

            cloudPass_->SetFrameData(inverse(proj), inverse(view), cam->GetPosition(), planetCentre, cloudSun,
                                     screenSize);
        }
    }
} // namespace SF::Engine
