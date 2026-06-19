#include "Scene.hpp"
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Mesh/MeshFactory.hpp>
#include <Graphics/Windows/Windows.hpp>
#include <ImGui/ImGuiPipelinePass.hpp>
#include <Graphics/Lighting/Lighting.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/AtmospherePipelinePass.hpp>
#include <Graphics/Visuals/Sun/SunPipelinePass.hpp>
#include <Graphics/Lighting/LightingTypes.hpp>
#include <Scene/Scene.hpp>

#include <Graphics/PipelinePassManager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <string>

#include <Scene/SceneRenderer.hpp>
#include <Graphics/RenderSystem.hpp>
#include <Bitmaps/Bitmap.hpp>
#include <Graphics/Images/ImageDepth.hpp>

#include <Scene/SceneManager.hpp>

#include <Gui/UIRegistry.hpp>

namespace SF::Engine
{
    Scene::Scene(std::unique_ptr<CameraController> &&cameraController, SceneRendererConfig cfg)
        : cameraController_(std::move(cameraController)), rendererCfg_(cfg)
    {
        MakeLight(
            lights_,
            "Sun",
            Lighting::LightType::Directional,
            {1.0f, 1.0f, 1.0f},
            3.0f,
            {0.0f, 5.0f, 0.0f},
            {0.0f, 0.0f, 0.0f});

        SceneObject &cube = objects_.emplace_back();
        cube.name = "Cube";
        cube.meshSourcePath = "__cube__";
        cube.material.baseColor = {0.72f, 0.72f, 0.78f, 1.0f};
        cube.material.roughnessFactor = 0.25f;
        cube.material.metallicFactor = 0.85f;

        lastFrameTime_ = std::chrono::steady_clock::now();
    }

    void Scene::Initialize()
    {
        if (initialized_)
            return;
        initialized_ = true;

        SceneRendererConfig cfg = rendererCfg_;
        cfg.enableAtmosphere = true;
        cfg.enableSun = true;

        cfg.atmosphereParams.bottomRadius = 6371000.0f;
        cfg.atmosphereParams.topRadius = 6471000.0f;
        cfg.atmosphereParams.sunIntensity = 40.0f;
        cfg.atmosphereParams.renderUnitRadius = cfg.atmosphereParams.bottomRadius;

        cfg.sunParams.intensity = 20.0f;
        cfg.sunParams.discAngleDeg = 0.8f;
        cfg.sunParams.haloAngleDeg = 5.0f;
        cfg.sunParams.haloStrength = 0.30f;
        cfg.sunParams.bloomStrength = 6.0f;

        auto ownedRenderer = std::make_unique<SceneRenderer>(cfg);
        sceneRenderer_ = ownedRenderer.get();

        if (auto *rs = RenderSystem::Get())
        {
            rs->SetRenderer(std::move(ownedRenderer));
            rs->ResetRenderStages();
            rs->GetRenderer()->Start();
            rs->GetRenderer()->SetStarted(true);
        }

        for (auto &obj : objects_)
        {
            if (!obj.mesh && !obj.meshSourcePath.empty())
            {
                if (obj.meshSourcePath == "__cube__")
                    obj.mesh = MeshFactory::CreateCube();
                else if (obj.meshSourcePath == "__sphere__")
                    obj.mesh = MeshFactory::CreateSphere();
                else if (obj.meshSourcePath == "__quad__")
                    obj.mesh = MeshFactory::CreateQuad();
            }
        }
    }

    void Scene::Update()
    {
        systems.ForEach([](auto, auto system)
                        {
            if (system->IsEnabled()) system->Update(); });
        entities.CleanupRemovedEntities();
    }

    void Scene::Render()
    {
        if (!litPass_ && sceneRenderer_)
        {
            lightManager_ = std::shared_ptr<LightManager>(
                sceneRenderer_->GetLightManager(), [](LightManager *) {});

            litPass_ = sceneRenderer_->GetLitPass();
            atmoPass_ = sceneRenderer_->GetAtmoPass();
            sunPass_ = sceneRenderer_->GetSunPass();
            cloudPass_ = sceneRenderer_->GetCloudPass();
            // todo: take ImGui out of Scene

            auto *imgui = sceneRenderer_->GetPipelinePass<ImGuiPipelinePass>();
            if (imgui)
                imgui->SetDrawCallback(
                    [this]()
                    {
                        ui_.Draw(
                            {cameraController_->GetActive(), &objects_, &lights_, &selectedObj_, &selectedLight_});
                        cloudPass_->DrawImGuiPanel();
                        UIRegistry::Get().DrawAll();
                    });
        }

        if (!litPass_ || !lightManager_)
            return;

        // ------------------------------------------------------------------ //
        // Delta time
        // ------------------------------------------------------------------ //
        auto now = std::chrono::steady_clock::now();
        float dt = std::min(std::chrono::duration<float>(now - lastFrameTime_).count(), 0.1f);
        lastFrameTime_ = now;
        elapsed_ += dt;

        // ------------------------------------------------------------------ //
        // Camera
        // ------------------------------------------------------------------ //
        auto *wnd = WindowManager::Get()->GetWindow(0);
        auto &io = ImGui::GetIO();
        cameraController_->SetFrameInput(wnd, io.WantCaptureMouse, io.WantCaptureKeyboard);
        cameraController_->Update(dt);
        ACamera *cam = cameraController_->GetActive();

        float aspect = wnd ? wnd->GetAspectRatio() : 1.0f;
        glm::vec2 screenSize = wnd ? glm::vec2(wnd->GetSize().x, wnd->GetSize().y)
                                   : glm::vec2(1280.0f, 720.0f);

        glm::mat4 view = cam->GetView();
        glm::mat4 proj = cam->GetProjection(aspect);

        SyncLightTransforms();
        RebuildLightManager();

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
        fd.time = elapsed_;
        fd.deltaTime = dt;
        fd.lightCount = sceneRenderer_->GetLightManager()->GetLightCount();
        fd.frameIndex = frameIndex_++;

        glm::vec3 sunDir = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f));
        glm::vec3 sunColor = glm::vec3(1.0f, 1.0, 1.0);
        float sunInt = 1.0f;
        if (!lights_.empty() &&
            lights_[0].light.type == Lighting::LightType::Directional)
        {
            glm::vec3 ld = glm::normalize(lights_[0].light.direction);
            sunDir = -ld;
            sunColor = lights_[0].light.color;
            sunInt = lights_[0].light.intensity;
        }
        fd.sunDirIntensity = glm::vec4(sunDir, sunInt);

        sceneRenderer_->GetLightManager()->Upload(fd);

        glm::vec3 planetCentre = {0.0f, -6371000.0f, 0.0f};

        for (auto &obj : objects_)
        {
            if (obj.enabled && obj.mesh)
                litPass_->Submit(obj.mesh, obj.material, obj.transform.ToMatrix());
        }

        if (atmoPass_)
        {
            glm::vec3 atmoSun = sunDir;
            if (!lights_.empty() &&
                lights_[0].light.type == Lighting::LightType::Directional)
            {
                glm::vec3 ld = glm::normalize(lights_[0].light.direction);
                if (glm::length(ld) > 0.5f)
                    atmoSun = -ld;
            }
            // Pass raw camera game-space position; SetFrameData subtracts planetCentre internally.
            atmoPass_->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                    cam->GetPosition(), planetCentre,
                                    atmoSun, screenSize);
        }

        if (sunPass_)
        {
            glm::vec3 discSun = sunDir;
            if (!lights_.empty() &&
                lights_[0].light.type == Lighting::LightType::Directional)
                discSun = -glm::normalize(lights_[0].light.direction);
            sunPass_->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                   discSun, screenSize);
        }
        if (cloudPass_)
        {
            glm::vec3 cloudSun = sunDir;
            if (!lights_.empty() &&
                lights_[0].light.type == Lighting::LightType::Directional)
            {
                glm::vec3 ld = glm::normalize(lights_[0].light.direction);
                if (glm::length(ld) > 0.5f)
                    cloudSun = -ld;
            }

            cloudPass_->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                     cam->GetPosition(), planetCentre,
                                     cloudSun, screenSize);
        }
    }

    void Scene::Serialize(XMLNode &node) const
    {
        node.SetAttribute("version", 1);
        for (const auto &obj : objects_)
        {
            XMLNode n = node.AddChild("Object");
            obj.Serialize(n);
        }
        for (const auto &sl : lights_)
        {
            XMLNode n = node.AddChild("Light");
            sl.Serialize(n);
        }
    }

    void Scene::Deserialize(const XMLNode &node)
    {
        objects_.clear();
        lights_.clear();

        for (XMLNode n : node.GetChildren("Object"))
        {
            SceneObject &obj = objects_.emplace_back();
            obj.Deserialize(n);
            if (!obj.meshSourcePath.empty())
            {
                if (obj.meshSourcePath == "__cube__")
                    obj.mesh = MeshFactory::CreateCube();
                else if (obj.meshSourcePath == "__sphere__")
                    obj.mesh = MeshFactory::CreateSphere();
                else if (obj.meshSourcePath == "__quad__")
                    obj.mesh = MeshFactory::CreateQuad();
            }
        }

        for (XMLNode n : node.GetChildren("Light"))
        {
            SceneLight &sl = lights_.emplace_back();
            sl.Deserialize(n);
        }

        RebuildLightManager();
    }

    void Scene::SaveXML(const std::string &filename)
    {
        if (auto *xml = XMLReader::Get())
            xml->Serialize("Scene", *this, filename);
    }

    void Scene::ReadXML(const std::string &filename)
    {
        if (auto *xml = XMLReader::Get())
            xml->Deserialize(filename, *this);
    }

    void Scene::ClearSystems() { systems.Clear(); }
    void Scene::ClearEntities() { entities.Clear(); }

    Entity Scene::GetEntity(const std::string &name) const { return entities.GetEntity(name); }
    Entity Scene::CreateEntity() { return entities.CreateEntity(); }
    Entity Scene::CreatePrefabEntity(const std::string &f) { return entities.CreatePrefabEntity(f); }
    std::vector<Entity> Scene::QueryAllEntities() { return entities.QueryAll(); }

    const ImageDepth *Scene::GetDepthTexture()
    {
        return dynamic_cast<const ImageDepth *>(RenderSystem::Get()->GetAttachment("gbuf_depth"));
    }

} // namespace SF::Engine
