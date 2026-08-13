#include "SceneRenderer.hpp"
#include <Rendering/RenderSystem.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Gui/ImGuiPipelinePass.hpp>
#include <Rendering/PipelinePassManager.hpp>
#include <Gui/UIRegistry.hpp>
#include <Scene/Scene.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Math/Transform.hpp>
#include <chrono>

namespace SF::Engine
{
    Image2d *SceneRenderer::GetHdrColorTarget()
    {
        auto *rs = RenderSystem::Get();
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

            if (GetPipelinePassManager()->Get<ImGuiPipelinePass>()) // if imgui exists
                GetPipelinePassManager()->Get<ImGuiPipelinePass>()->SetDrawCallback(
                    [this, scene]()
                    {
                        UIRegistry::Get().DrawAll();
                    });
        }

        auto now = std::chrono::steady_clock::now();
        float dt = std::min(std::chrono::duration<float>(now - scene->lastFrameTime_).count(), 0.1f);
        scene->lastFrameTime_ = now;
        scene->elapsed_ += dt;

        auto *wnd = WindowManager::Get()->GetWindow(0);
        auto &io = ImGui::GetIO();

        CameraController *cameraController = scene->GetCamera();
        cameraController->SetFrameInput(wnd, io.WantCaptureMouse, io.WantCaptureKeyboard);
        cameraController->Update(dt);
        Camera *cam = cameraController->GetActive();

        float aspect = wnd ? wnd->GetAspectRatio() : 1.0f;
        Vec2 screenSize = wnd ? Vec2(wnd->GetSize().x, wnd->GetSize().y)
                              : Vec2(800.0f, 600.0f);

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
        fd.view = view;
        fd.proj = proj;
        fd.viewProj = proj * view;
        fd.invView = inverse(view);
        fd.invProj = inverse(proj);
        fd.invViewProj = inverse(fd.viewProj);
        fd.cameraPos = Vec4(cam->GetPosition(), cam->GetNearPlane());
        fd.cameraDir = Vec4(cam->GetFront(), cam->GetFarPlane());
        fd.screenSize = screenSize;
        fd.invScreenSize = 1.0f / screenSize;
        fd.nearPlane = cam->GetNearPlane();
        fd.farPlane = cam->GetFarPlane();
        fd.time = scene->elapsed_;
        fd.deltaTime = dt;
        fd.lightCount = lightManager_->GetLightCount();
        fd.frameIndex = scene->frameIndex_++;

        Vec3 sunDir = normalize(Vec3(0.0f, 0.0f, 0.0f));
        Vec3 sunColor = Vec3(1.0f, 1.0f, 1.0f);
        float sunInt = 1.0f;
        if (!scene->lights_.empty() &&
            scene->lights_[0]->GetComponent<Light>()->type == Lighting::LightType::Directional)
        {
            Vec3 ld = normalize(scene->lights_[0]->GetComponent<Light>()->direction);
            sunDir = -ld;
            sunColor = scene->lights_[0]->GetComponent<Light>()->color;
            sunInt = scene->lights_[0]->GetComponent<Light>()->intensity;
        }
        fd.sunDirIntensity = Vec4(sunDir, sunInt);

        lightManager_->Upload(fd);

        // Legacy single-planet centre, still used by CloudPipelinePass until it's
        // converted to per-planet data like AtmosphereController.
        Vec3 planetCentre = {0.0f, -6371000.0f, 0.0f};

        for (auto &obj : scene->objects_)
        {
            if (obj->enabled && obj->mesh)
                gbuffer_->Submit(obj->mesh, *obj->GetComponent<MeshMaterial>(), obj->GetComponent<Transform>()->ToMatrix());
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
            atmoController->SetFrameData(inverse(proj), inverse(view),
                                         cam->GetPosition(), atmoSun, screenSize);
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

            cloudPass_->SetFrameData(inverse(proj), inverse(view),
                                     cam->GetPosition(), planetCentre,
                                     cloudSun, screenSize);
        }
    }
}