#include "SceneRenderer.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Windows/WindowManager.hpp>
#include <ImGui/ImGuiPipelinePass.hpp>
#include <Graphics/PipelinePassManager.hpp>
#include <Gui/UIRegistry.hpp>
#include <Scene/Scene.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        if (!litPass_ || !lightManager_ || !scene)
            return;

        if (!uiCallbackSet_)
        {
            uiCallbackSet_ = true;

            if (GetPipelinePassManager()->Get<ImGuiPipelinePass>()) // if imgui exists
                GetPipelinePassManager()->Get<ImGuiPipelinePass>()->SetDrawCallback(
                    [this, scene]()
                    {
                        scene->ui_.Draw({scene->GetCamera()->GetActive(), &scene->objects_, &scene->lights_,
                                         &scene->selectedObj_, &scene->selectedLight_});
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
        glm::vec2 screenSize = wnd ? glm::vec2(wnd->GetSize().x, wnd->GetSize().y)
                                   : glm::vec2(1280.0f, 720.0f);

        glm::mat4 view = cam->GetView();
        glm::mat4 proj = cam->GetProjection(aspect);

        scene->SyncLightTransforms();
        scene->RebuildLightManager();

        Lighting::GpuFrameData fd{};
        fd.view = view;
        fd.proj = proj;
        fd.viewProj = proj * view;
        fd.invView = glm::inverse(view);
        fd.invProj = glm::inverse(proj);
        fd.invViewProj = glm::inverse(fd.viewProj);
        fd.cameraPos = glm::vec4(cam->GetPosition(), cam->GetNearPlane());
        fd.cameraDir = glm::vec4(cam->GetFront(), cam->GetFarPlane());
        fd.screenSize = screenSize;
        fd.invScreenSize = 1.0f / screenSize;
        fd.nearPlane = cam->GetNearPlane();
        fd.farPlane = cam->GetFarPlane();
        fd.time = scene->elapsed_;
        fd.deltaTime = dt;
        fd.lightCount = lightManager_->GetLightCount();
        fd.frameIndex = scene->frameIndex_++;

        glm::vec3 sunDir = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f));
        glm::vec3 sunColor = glm::vec3(1.0f, 1.0f, 1.0f);
        float sunInt = 1.0f;
        if (!scene->lights_.empty() &&
            scene->lights_[0].light.type == Lighting::LightType::Directional)
        {
            glm::vec3 ld = glm::normalize(scene->lights_[0].light.direction);
            sunDir = -ld;
            sunColor = scene->lights_[0].light.color;
            sunInt = scene->lights_[0].light.intensity;
        }
        fd.sunDirIntensity = glm::vec4(sunDir, sunInt);

        lightManager_->Upload(fd);

        // Legacy single-planet centre, still used by CloudPipelinePass until it's
        // converted to per-planet data like AtmosphereController.
        glm::vec3 planetCentre = {0.0f, -6371000.0f, 0.0f};

        for (auto &obj : scene->objects_)
        {
            if (obj.enabled && obj.mesh)
                litPass_->Submit(obj.mesh, obj.material, obj.transform.ToMatrix());
        }

        if (atmoController && !atmoController->Empty())
        {
            glm::vec3 atmoSun = sunDir;
            if (!scene->lights_.empty() &&
                scene->lights_[0].light.type == Lighting::LightType::Directional)
            {
                glm::vec3 ld = glm::normalize(scene->lights_[0].light.direction);
                if (glm::length(ld) > 0.5f)
                    atmoSun = -ld;
            }
            atmoController->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                          cam->GetPosition(), atmoSun, screenSize);
        }

        if (cloudPass_)
        {
            glm::vec3 cloudSun = sunDir;
            if (!scene->lights_.empty() &&
                scene->lights_[0].light.type == Lighting::LightType::Directional)
            {
                glm::vec3 ld = glm::normalize(scene->lights_[0].light.direction);
                if (glm::length(ld) > 0.5f)
                    cloudSun = -ld;
            }

            cloudPass_->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                     cam->GetPosition(), planetCentre,
                                     cloudSun, screenSize);
        }
    }
}