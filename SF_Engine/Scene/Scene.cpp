#include "Scene.hpp"
#include <Rendering/Renderer.hpp>
#include <Rendering/Stage.hpp>
#include <Rendering/Mesh/MeshFactory.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Gui/ImGuiPipelinePass.hpp>
#include <Rendering/Lighting/Lighting.hpp>
#include <Rendering/Visuals/sfSkies/Atmosphere/AtmospherePipelinePass.hpp>
#include <Rendering/Lighting/LightingTypes.hpp>
#include <Scene/Scene.hpp>

#include <Rendering/PipelinePassManager.hpp>

#include <Math/BasicMath.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <string>

#include <Scene/SceneRenderer.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Assets/Bitmaps/Bitmap.hpp>
#include <Rendering/Images/ImageDepth.hpp>

#include <Scene/SceneManager.hpp>
#include <Gui/UIRegistry.hpp>

namespace SF::Engine
{
    Scene::Scene(std::unique_ptr<CameraController> &&cameraController, std::string name, SceneRendererConfig cfg)
        : cameraController_(std::move(cameraController)), rendererCfg_(cfg)
    {
        AddLight("Sun", Lighting::LightType::Directional,
                 {1.0f, 1.0f, 1.0f}, 3.0f,
                 {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

        AddLight("Point", Lighting::LightType::Point, {1,1,1}, 10.0, {0, 20, 0}, {0,0,0});

        SceneObject *cube = AddObject("Cube");
        cube->meshSourcePath = "__cube__";
        cube->GetComponent<MeshMaterial>()->baseColor = {0.72f, 0.72f, 0.78f, 1.0f};
        cube->GetComponent<MeshMaterial>()->roughnessFactor = 0.25f;
        cube->GetComponent<MeshMaterial>()->metallicFactor = 0.85f;
        cube->GetComponent<Transform>()->position = {0.0f, 2.0f, 0.0f};

        SceneObject* floor = AddObject("Cube 2");
        floor->meshSourcePath = "__cube__";
        floor->GetComponent<MeshMaterial>()->baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        floor->GetComponent<MeshMaterial>()->roughnessFactor = 0.0f;
        floor->GetComponent<MeshMaterial>()->metallicFactor = 0.0f;
        floor->GetComponent<Transform>()->scale = {1000.0f, 1.0f, 1000.0f};

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

        // ImGui must target the stage that actually owns "swapchain" — that's
        // now stage 2 (its own render pass; see SceneRenderer.hpp for why
        // tonemap/swapchain was split out of stage 1).
        ImGuiPipelinePass::SetTargetStage(Pipeline::Stage{2, 0});

        if (auto *rs = RenderSystem::Get())
        {
            rs->SetRenderer(std::move(ownedRenderer));
            rs->ResetRenderStages();
            rs->GetRenderer()->Start();
            rs->GetRenderer()->SetStarted(true);
        }

        for (auto *obj : objects_)
        {
            if (!obj->mesh && !obj->meshSourcePath.empty())
            {
                if (obj->meshSourcePath == "__cube__")
                    obj->mesh = MeshFactory::CreateCube();
                else if (obj->meshSourcePath == "__sphere__")
                    obj->mesh = MeshFactory::CreateSphere();
                else if (obj->meshSourcePath == "__quad__")
                    obj->mesh = MeshFactory::CreateQuad();
            }
        }
    }

    SceneObject *Scene::AddObject(const std::string &name, Entity *parent)
    {
        SceneObject *ptr = parent
                               ? entities.CreateChildEntity<SceneObject>(parent, name)
                               : entities.CreateEntity<SceneObject>(name);
        objects_.push_back(ptr);
        return ptr;
    }

    SceneObject *Scene::AddObject(const std::string &name, Transform &transform, Entity *parent)
    {
        SceneObject *ptr = parent
                               ? entities.CreateChildEntity<SceneObject>(parent, name)
                               : entities.CreateEntity<SceneObject>(name);

        auto A = ptr->GetComponent<Transform>();
        A->position = transform.position;
        A->rotation = transform.rotation;
        A->scale    = transform.scale;

        objects_.push_back(ptr);
        return ptr;
    }

    SceneLight *Scene::AddLight(const std::string &name, Lighting::LightType type,
                                const Vec3 &color, float intensity,
                                const Vec3 &position, const Vec3 &rotation,
                                Entity *parent)
    {
        SceneLight *ptr = parent
                              ? entities.CreateChildEntity<SceneLight>(parent, name)
                              : entities.CreateEntity<SceneLight>(name);

        ptr->GetComponent<Transform>()->position = position;
        ptr->GetComponent<Transform>()->rotation = rotation;
        ptr->GetComponent<Light>()->type = type;
        ptr->GetComponent<Light>()->color = color;
        ptr->GetComponent<Light>()->intensity = intensity;
        ptr->GetComponent<Light>()->name = name;

        lights_.push_back(ptr);
        SyncLightTransforms();
        RebuildLightManager();
        return ptr;
    }

    SceneLight *Scene::AddLight(const std::string &name, Lighting::LightType type,
                                const Vec3 &color, float intensity,
                                Transform &transform,
                                Entity *parent)
    {
        SceneLight *ptr = parent
                              ? entities.CreateChildEntity<SceneLight>(parent, name)
                              : entities.CreateEntity<SceneLight>(name);

        ptr->GetComponent<Transform>()->position = transform.position;
        ptr->GetComponent<Transform>()->rotation = transform.rotation;
        ptr->GetComponent<Light>()->type = type;
        ptr->GetComponent<Light>()->color = color;
        ptr->GetComponent<Light>()->intensity = intensity;
        ptr->GetComponent<Light>()->name = name;

        lights_.push_back(ptr);
        SyncLightTransforms();
        RebuildLightManager();
        return ptr;
    }

    void Scene::RemoveEntitySubtree(Entity *rootEntity)
    {
        if (!rootEntity)
            return;

        // Collect the whole subtree first: DestroyEntity below cascades through
        // children via unique_ptr, so raw pointers under root dangle the moment
        // it's called — walk before destroying, not during.
        std::vector<Entity *> subtree;
        std::function<void(Entity *)> collect = [&](Entity *e)
        {
            subtree.push_back(e);
            for (auto &c : e->GetChildren())
                collect(c.get());
        };
        collect(rootEntity);

        for (Entity *e : subtree)
        {
            if (auto *obj = dynamic_cast<SceneObject *>(e))
                objects_.erase(std::remove(objects_.begin(), objects_.end(), obj), objects_.end());
            else if (auto *light = dynamic_cast<SceneLight *>(e))
                lights_.erase(std::remove(lights_.begin(), lights_.end(), light), lights_.end());
        }

        entities.GetRegistry().DestroyEntity(rootEntity);
        RebuildLightManager();
    }

    void Scene::RemoveObject(SceneObject *obj)
    {
        if (obj)
            RemoveEntitySubtree(obj);
    }
    void Scene::RemoveLight(SceneLight *light)
    {
        if (light)
            RemoveEntitySubtree(light);
    }

    std::pair<int, int> Scene::FindParentRef(Entity *e) const
    {
        Entity *p = e ? e->GetParent() : nullptr;
        if (!p)
            return {0, -1};
        if (auto *obj = dynamic_cast<SceneObject *>(p))
        {
            auto it = std::find(objects_.begin(), objects_.end(), obj);
            if (it != objects_.end())
                return {1, (int)std::distance(objects_.begin(), it)};
        }
        if (auto *light = dynamic_cast<SceneLight *>(p))
        {
            auto it = std::find(lights_.begin(), lights_.end(), light);
            if (it != lights_.end())
                return {2, (int)std::distance(lights_.begin(), it)};
        }
        return {0, -1};
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
        node.SetAttribute("version", 2);
        for (size_t i = 0; i < objects_.size(); i++)
        {
            XMLNode n = node.AddChild("Object");
            auto [kind, idx] = FindParentRef(objects_[i]);
            n.SetAttribute("parentKind", kind);
            n.SetAttribute("parentIndex", idx);
            objects_[i]->Serialize(n);
        }
        for (size_t i = 0; i < lights_.size(); i++)
        {
            XMLNode n = node.AddChild("Light");
            auto [kind, idx] = FindParentRef(lights_[i]);
            n.SetAttribute("parentKind", kind);
            n.SetAttribute("parentIndex", idx);
            lights_[i]->Serialize(n);
        }
    }

    void Scene::Deserialize(const XMLNode &node)
    {
        objects_.clear();
        lights_.clear();
        entities.Clear();

        struct PendingParent
        {
            Entity *entity;
            int kind;
            int index;
        };
        std::vector<PendingParent> pending;

        for (XMLNode n : node.GetChildren("Object"))
        {
            SceneObject *obj = entities.CreateEntity<SceneObject>(std::string()); // name set by Deserialize below
            obj->Deserialize(n);
            if (!obj->meshSourcePath.empty())
            {
                if (obj->meshSourcePath == "__cube__")
                    obj->mesh = MeshFactory::CreateCube();
                else if (obj->meshSourcePath == "__sphere__")
                    obj->mesh = MeshFactory::CreateSphere();
                else if (obj->meshSourcePath == "__quad__")
                    obj->mesh = MeshFactory::CreateQuad();
            }

            int parentKind = 0, parentIndex = -1;
            n.GetAttribute("parentKind", parentKind);
            n.GetAttribute("parentIndex", parentIndex);
            pending.push_back({obj, parentKind, parentIndex});

            objects_.push_back(obj);
        }

        for (XMLNode n : node.GetChildren("Light"))
        {
            SceneLight *light = entities.CreateEntity<SceneLight>(std::string());
            light->Deserialize(n);

            int parentKind = 0, parentIndex = -1;
            n.GetAttribute("parentKind", parentKind);
            n.GetAttribute("parentIndex", parentIndex);
            pending.push_back({light, parentKind, parentIndex});

            lights_.push_back(light);
        }

        // Reparent in a second pass, once every Entity in this batch exists —
        // an object can reference a parent that's deserialized later in the file.
        for (auto &p : pending)
        {
            if (p.kind == 1 && p.index >= 0 && p.index < (int)objects_.size())
                entities.GetRegistry().Reparent(p.entity, objects_[p.index]);
            else if (p.kind == 2 && p.index >= 0 && p.index < (int)lights_.size())
                entities.GetRegistry().Reparent(p.entity, lights_[p.index]);
        }

        RebuildLightManager();
    }

    void Scene::SaveXML(const std::string &filename)
    {
        if (auto *xml = XMLModule::Get())
            xml->Serialize("Scene", *this, filename);
    }

    void Scene::ReadXML(const std::string &filename)
    {
        if (auto *xml = XMLModule::Get())
            xml->Deserialize(filename, *this);
    }

    void Scene::ClearSystems() { systems.Clear(); }
    void Scene::ClearEntities() { entities.Clear(); }

    Entity *Scene::GetEntity(const std::string &name) const { return entities.GetEntity(name); }
    Entity *Scene::GetEntity(const EntityId &id) { return entities.FindById(id); } // doesnt like const
    Entity *Scene::CreateEntity() { return entities.CreateEntity(); }
    std::vector<Entity *> Scene::QueryAllEntities() { return entities.QueryAll(); }

    const ImageDepth *Scene::GetDepthTexture()
    {
        return dynamic_cast<const ImageDepth *>(RenderSystem::Get()->GetAttachment("gbuf_depth"));
    }

    Scene::~Scene()
    {
        ClearSystems();
        ClearEntities();
    }

} // namespace SF::Engine
