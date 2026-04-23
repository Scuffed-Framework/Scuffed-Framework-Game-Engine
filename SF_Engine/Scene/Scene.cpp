#include "Scene.hpp"
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Mesh/MeshFactory.hpp>
#include <Graphics/Windows/Windows.hpp>
#include <ImGui/ImGuiPipelinePass.hpp>
#include <Graphics/Lighting/Lighting.hpp>
#include <Graphics/Visuals/Atmosphere/AtmospherePipelinePass.hpp>
#include <Graphics/Visuals/Sun/SunPipelinePass.hpp>
#include <Graphics/Visuals/Clouds/CloudPipelinePass.hpp>
#include <Graphics/Lighting/LightingTypes.hpp>
#include <Scene/Scene.hpp>

#include <Graphics/PipelinePassManager.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <string>

#include <Scene/SceneRenderer.hpp>
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    Scene::Scene(std::unique_ptr<ACamera> &&camera, SceneRendererConfig cfg)
        : camera(std::move(camera)), rendererCfg_(cfg)
    {
        auto MakeLight = [&](const char *name,
                             Lighting::LightType type,
                             glm::vec3 color, float intensity,
                             glm::vec3 pos, glm::vec3 rotDeg,
                             float radius = 10.0f) -> SceneLight &
        {
            SceneLight &sl = lights_.emplace_back();
            sl.name = name;
            sl.light.name = name;
            sl.light.type = type;
            sl.light.color = color;
            sl.light.intensity = intensity;
            sl.light.radius = radius;
            sl.light.castShadow = true;
            sl.transform.position = pos;
            sl.transform.rotation = rotDeg;
            if (type == Lighting::LightType::Directional)
                sl.light.direction = glm::normalize(glm::vec3(0.5f, -0.259f, 0.827f));
            return sl;
        };

        MakeLight("Sun", Lighting::LightType::Directional,
                  {1.0f, 0.95f, 0.85f}, 3.0f,
                  {0, 5, 0}, {0.0f, 0.0f, 0.0f});

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
        cfg.enableAtmosphere = cfg.enableAtmosphere || atmosphereEnabled;
        cfg.enableSun = cfg.enableSun || sunEnabled;
        cfg.enableClouds = cfg.enableClouds || cloudsEnabled;

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
        // Resolve pass pointers on first Render() after Initialize().
        if (!litPass_ && sceneRenderer_)
        {
            lightManager_ = std::shared_ptr<LightManager>(
                sceneRenderer_->GetLightManager(), [](LightManager *) {});
            litPass_ = sceneRenderer_->GetLitPass();
            atmoPass_ = sceneRenderer_->GetAtmoPass();
            sunPass_ = sceneRenderer_->GetSunPass();
            cloudPass_ = sceneRenderer_->GetCloudPass();

            auto *imgui = sceneRenderer_->GetPipelinePass<ImGuiPipelinePass>();
            if (imgui)
                imgui->SetDrawCallback([this]()
                                       { ui_.Draw({camera.get(), &objects_, &lights_,
                                                   &selectedObj_, &selectedLight_}); });
        }

        if (!litPass_ || !lightManager_)
            return;

        // Delta time
        auto now = std::chrono::steady_clock::now();
        float dt = std::min(std::chrono::duration<float>(now - lastFrameTime_).count(), 0.1f);
        lastFrameTime_ = now;
        elapsed_ += dt;

        // Camera
        auto *wnd = WindowManager::Get()->GetWindow(0);
        auto &io = ImGui::GetIO();
        camera->Update(wnd, dt, io.WantCaptureMouse, io.WantCaptureKeyboard);

        float aspect = wnd ? wnd->GetAspectRatio() : 1.0f;
        glm::vec2 screenSize = wnd ? glm::vec2(wnd->GetSize().x, wnd->GetSize().y)
                                   : glm::vec2(800.0f, 600.0f);

        glm::mat4 view = camera->GetView();
        glm::mat4 proj = camera->GetProjection(aspect);

        SyncLightTransforms();
        RebuildLightManager();

        // Frame data
        Lighting::GpuFrameData fd{};
        fd.view = view;
        fd.proj = proj;
        fd.viewProj = proj * view;
        fd.invView = glm::inverse(view);
        fd.invProj = glm::inverse(proj);
        fd.invViewProj = glm::inverse(fd.viewProj);
        fd.cameraPos = glm::vec4(camera->GetPosition(), camera->GetNearPlane());
        fd.cameraDir = glm::vec4(camera->GetFront(), camera->GetFarPlane());
        fd.screenSize = screenSize;
        fd.invScreenSize = 1.0f / screenSize;
        fd.nearPlane = camera->GetNearPlane();
        fd.farPlane = camera->GetFarPlane();
        fd.time = elapsed_;
        fd.deltaTime = dt;
        fd.lightCount = sceneRenderer_->GetLightManager()->GetLightCount();
        fd.frameIndex = frameIndex_++;

        // Sun direction (safe default  non-zero)
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.0f, 0.707f, -0.707f));
        float sunInt = 1.0f;
        if (!lights_.empty() &&
            lights_[0].light.type == Lighting::LightType::Directional)
        {
            glm::vec3 ld = glm::normalize(lights_[0].light.direction);
            sunDir = -ld;
            sunInt = lights_[0].light.intensity;
        }
        fd.sunDirIntensity = glm::vec4(sunDir, sunInt);

        sceneRenderer_->GetLightManager()->Upload(fd);

        // Meshes
        for (auto &obj : objects_)
        {
            if (obj.enabled && obj.mesh)
                litPass_->Submit(obj.mesh, obj.material, obj.transform.ToMatrix());
        }

        // Atmosphere
        if (atmoPass_)
        {
            glm::vec3 planetCentre = {0.0f, -6371000.0f, 0.0f};
            glm::vec3 atmoSun = sunDir;
            if (!lights_.empty() &&
                lights_[0].light.type == Lighting::LightType::Directional)
            {
                glm::vec3 ld = glm::normalize(lights_[0].light.direction);
                if (glm::length(ld) > 0.5f)
                    atmoSun = -ld;
            }
            atmoPass_->SetFrameData(glm::inverse(proj), glm::inverse(view),
                                    camera->GetPosition(), planetCentre, atmoSun, screenSize);
        }

        // Sun disc
        if (sunPass_)
        {
            glm::vec3 discSun = sunDir;
            if (!lights_.empty() &&
                lights_[0].light.type == Lighting::LightType::Directional)
                discSun = -glm::normalize(lights_[0].light.direction);
            sunPass_->SetFrameData(glm::inverse(proj), glm::inverse(view), discSun, screenSize);
        }

        // Volumetric clouds
        if (cloudPass_)
        {
            // Resize the half-res offscreen buffer when the window changes size.
            uint32_t sw = static_cast<uint32_t>(screenSize.x);
            uint32_t sh = static_cast<uint32_t>(screenSize.y);
            if (sw != lastScreenW_ || sh != lastScreenH_)
            {
                cloudPass_->OnResize(sw, sh);
                lastScreenW_ = sw;
                lastScreenH_ = sh;
            }

            cloudPass_->SetFrameData(fd.invViewProj, camera->GetPosition(), sunDir, dt);
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
}
