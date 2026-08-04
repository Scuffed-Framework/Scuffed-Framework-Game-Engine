#include "Scene.hpp"
#include <Graphics/Renderer.hpp>
#include <Graphics/Stage.hpp>
#include <Graphics/Mesh/MeshFactory.hpp>
#include <Graphics/Windows/WindowManager.hpp>
#include <ImGui/ImGuiPipelinePass.hpp>
#include <Graphics/Lighting/Lighting.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/AtmospherePipelinePass.hpp>
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
    Scene::Scene(std::unique_ptr<CameraController> &&cameraController, std::string name, SceneRendererConfig cfg)
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

        cfg.atmosphereParams.bottomRadius = 6371000.0f;
        cfg.atmosphereParams.topRadius = 6471000.0f;
        cfg.atmosphereParams.sunIntensity = 40.0f;
        cfg.atmosphereParams.renderUnitRadius = cfg.atmosphereParams.bottomRadius;

        auto ownedRenderer = std::make_unique<SceneRenderer>(cfg);
        sceneRenderer_ = ownedRenderer.get();

        ImGuiPipelinePass::SetTargetStage(Pipeline::Stage{1, 0}); // swapchain lives in stage 1 now

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
        if (sceneRenderer_)
            sceneRenderer_->RenderScene(this);
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

    Entity* Scene::GetEntity(const std::string &name) const { return entities.GetEntity(name); }
    Entity* Scene::CreateEntity() { return entities.CreateEntity(); }
    Entity* Scene::CreatePrefabEntity(const std::string &f) { return entities.CreatePrefabEntity(f); }
    std::vector<Entity*> Scene::QueryAllEntities() { return entities.QueryAll(); }

    const ImageDepth *Scene::GetDepthTexture()
    {
        return dynamic_cast<const ImageDepth *>(RenderSystem::Get()->GetAttachment("gbuf_depth"));
    }

} // namespace SF::Engine
