#include "Scene.hpp"
#include <Gui/ImGui/ImGuiSFERenderpass.hpp>
#include <Platform/Windowing/WindowManager.hpp>
#include <Rendering/FrameGraph/Stage.hpp>
#include <Rendering/Lighting/Lighting.hpp>
#include <Rendering/Lighting/LightingTypes.hpp>
#include <Rendering/Mesh/MeshFactory.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Visuals/Sky/Atmosphere/AtmosphereSFERenderpass.hpp>

#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>

#include <Math/BasicMath.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

#include <Assets/Bitmaps/Bitmap.hpp>
#include <Rendering/RenderSystem.hpp>

#include <Gui/ImGui/UIRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <utility>

namespace SF::Engine
{
    Scene::Scene(std::unique_ptr<CameraController> &&cameraController, std::string name, const SceneRendererConfig &cfg,
                 bool populateDefaults) :
        rendererCfg_(cfg), cameraController_(std::move(cameraController)), name(std::move(name))
    {
        if (populateDefaults)
            PopulateDefaults();

        lastFrameTime_ = std::chrono::steady_clock::now();
    }

    void Scene::ResolveBuiltinMesh(SceneObject *obj)
    {
        if (!obj || obj->mesh || obj->meshSourcePath.empty())
            return;
        if (obj->meshSourcePath == "__cube__")
            obj->mesh = MeshFactory::CreateCube();
        else if (obj->meshSourcePath == "__sphere__")
            obj->mesh = MeshFactory::CreateSphere();
        else if (obj->meshSourcePath == "__quad__")
            obj->mesh = MeshFactory::CreateQuad();
        // Non-builtin mesh paths are expected to be loaded by the asset layer.
    }

    void Scene::PopulateDefaults()
    {
        AddLight("Sun", Lighting::LightType::Directional, {1.0f, 1.0f, 1.0f}, 3.0f, {0.0f, 5.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
        AddLight("Point", Lighting::LightType::Point, {1, 1, 1}, 10.0, {0, 20, 0}, {0, 0, 0});

        SceneObject *cube                                   = AddObject("Cube");
        cube->meshSourcePath                                = "__cube__";
        cube->GetComponent<MeshMaterial>()->baseColor       = {0.72f, 0.72f, 0.78f, 1.0f};
        cube->GetComponent<MeshMaterial>()->roughnessFactor = 0.25f;
        cube->GetComponent<MeshMaterial>()->metallicFactor  = 0.85f;
        cube->GetComponent<Transform>()->position           = {0.0f, 2.0f, 0.0f};

        SceneObject *floor                                   = AddObject("Cube 2");
        floor->meshSourcePath                                = "__cube__";
        floor->GetComponent<MeshMaterial>()->baseColor       = {1.0f, 1.0f, 1.0f, 1.0f};
        floor->GetComponent<MeshMaterial>()->roughnessFactor = 0.0f;
        floor->GetComponent<MeshMaterial>()->metallicFactor  = 0.0f;
        floor->GetComponent<Transform>()->scale              = {1000.0f, 1.0f, 1000.0f};

        // Meshes are normally created lazily in Initialize(); if we're already
        // initialized (e.g. loading an empty file at runtime) do it now.
        if (initialized_)
        {
            ResolveBuiltinMesh(cube);
            ResolveBuiltinMesh(floor);
        }
    }

    void Scene::Initialize()
    {
        if (initialized_)
            return;

        auto *rs = RenderSystem::Get();
        if (!rs)
            return; // not ready yet; SceneManager::Update retries next frame

        initialized_ = true;

        SceneRendererConfig cfg = rendererCfg_;
        if (atmosphereEnabled && !cfg.enableAtmosphere)
        {
            cfg.enableAtmosphere                  = true;
            cfg.atmosphereParams.bottomRadius     = 6371000.0f;
            cfg.atmosphereParams.topRadius        = 6471000.0f;
            cfg.atmosphereParams.sunIntensity     = 40.0f;
            cfg.atmosphereParams.renderUnitRadius = cfg.atmosphereParams.bottomRadius;
        }

        auto ownedRenderer = std::make_unique<SceneRenderer>(cfg);
        sceneRenderer_     = ownedRenderer.get(); // only assigned once ownership is guaranteed below
        ImGuiSFERenderpass::SetTargetStage(Pipeline::Stage{2, 0});

        rs->SetRenderer(std::move(ownedRenderer));
        rs->ResetRenderStages();
        rs->GetRenderer()->Start();
        rs->GetRenderer()->SetStarted(true);

        for (auto *obj: objects_)
            ResolveBuiltinMesh(obj);

        SyncLightTransforms();
        RebuildLightManager();
    }

    SceneObject *Scene::AddObject(const std::string &name, Entity *parent)
    {
        SceneObject *ptr = parent ? entities.CreateChildEntity<SceneObject>(parent, name)
                                  : entities.CreateEntity<SceneObject>(name);
        objects_.push_back(ptr);
        return ptr;
    }

    SceneObject *Scene::AddObject(const std::string &name, Transform &transform, Entity *parent)
    {
        SceneObject *ptr = parent ? entities.CreateChildEntity<SceneObject>(parent, name)
                                  : entities.CreateEntity<SceneObject>(name);

        auto A      = ptr->GetComponent<Transform>();
        A->position = transform.position;
        A->rotation = transform.rotation;
        A->scale    = transform.scale;

        objects_.push_back(ptr);
        return ptr;
    }

    SceneLight *Scene::AddLight(const std::string &name, Lighting::LightType type, const Vec3 &color, float intensity,
                                const Vec3 &position, const Vec3 &rotation, Entity *parent)
    {
        SceneLight *ptr =
                parent ? entities.CreateChildEntity<SceneLight>(parent, name) : entities.CreateEntity<SceneLight>(name);

        ptr->GetComponent<Transform>()->position = position;
        ptr->GetComponent<Transform>()->rotation = rotation;
        ptr->GetComponent<Light>()->type         = type;
        ptr->GetComponent<Light>()->color        = color;
        ptr->GetComponent<Light>()->intensity    = intensity;
        ptr->GetComponent<Light>()->name         = name;

        lights_.push_back(ptr);
        SyncLightTransforms();
        RebuildLightManager();
        return ptr;
    }

    SceneLight *Scene::AddLight(const std::string &name, Lighting::LightType type, const Vec3 &color, float intensity,
                                Transform &transform, Entity *parent)
    {
        SceneLight *ptr =
                parent ? entities.CreateChildEntity<SceneLight>(parent, name) : entities.CreateEntity<SceneLight>(name);

        ptr->GetComponent<Transform>()->position = transform.position;
        ptr->GetComponent<Transform>()->rotation = transform.rotation;
        ptr->GetComponent<Light>()->type         = type;
        ptr->GetComponent<Light>()->color        = color;
        ptr->GetComponent<Light>()->intensity    = intensity;
        ptr->GetComponent<Light>()->name         = name;

        lights_.push_back(ptr);
        SyncLightTransforms();
        RebuildLightManager();
        return ptr;
    }

    void Scene::PurgeFromCaches(Entity *rootEntity)
    {
        if (!rootEntity)
            return;

        std::vector<Entity *> subtree;
        std::function<void(Entity *)> collect = [&](Entity *e)
        {
            subtree.push_back(e);
            for (auto &c: e->GetChildren())
                collect(c.get());
        };
        collect(rootEntity);

        for (Entity *e: subtree)
        {
            if (auto *obj = dynamic_cast<SceneObject *>(e))
                objects_.erase(std::remove(objects_.begin(), objects_.end(), obj), objects_.end());
            else if (auto *light = dynamic_cast<SceneLight *>(e))
                lights_.erase(std::remove(lights_.begin(), lights_.end(), light), lights_.end());
        }
    }

    void Scene::RemoveEntitySubtree(Entity *rootEntity)
    {
        if (!rootEntity)
            return;

        PurgeFromCaches(rootEntity);
        entities.GetRegistry().DestroyEntity(rootEntity);
        RebuildLightManager();
    }

    void Scene::RemoveEntity(Entity *entity)
    {
        if (!entity)
            return;

        PurgeFromCaches(entity);
        entities.Remove(entity);
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

    std::pair<int, int> Scene::FindParentRef(Entity *e, const IndexMap &idx)
    {
        // Walk up past plain (unserialized) entities to the nearest serialized ancestor,
        // so hierarchy survives a round trip instead of silently flattening to root.
        for (Entity *p = e ? e->GetParent() : nullptr; p; p = p->GetParent())
        {
            auto it = idx.find(p);
            if (it != idx.end())
                return it->second;
        }
        return {0, -1};
    }

    void Scene::Update()
    {
        systems.ForEach(
                [](auto, auto system)
                {
                    if (system->IsEnabled())
                        system->Update();
                });
        entities.CleanupRemovedEntities();
    }

    void Scene::Render()
    {
        if (sceneRenderer_)
            sceneRenderer_->RenderScene(this);
    }

    void Scene::Serialize(XMLNode &node) const
    {
        node.SetAttribute("version", kSceneVersion);
        if (!name.empty())
            node.SetAttribute("name", name);

        IndexMap idx;
        idx.reserve(objects_.size() + lights_.size());
        for (size_t i = 0; i < objects_.size(); i++)
            idx[objects_[i]] = {1, (int) i};
        for (size_t i = 0; i < lights_.size(); i++)
            idx[lights_[i]] = {2, (int) i};

        for (size_t i = 0; i < objects_.size(); i++)
        {
            XMLNode n       = node.AddChild("Object");
            auto [kind, pi] = FindParentRef(objects_[i], idx);
            n.SetAttribute("parentKind", kind);
            n.SetAttribute("parentIndex", pi);
            objects_[i]->Serialize(n);
        }
        for (size_t i = 0; i < lights_.size(); i++)
        {
            XMLNode n       = node.AddChild("Light");
            auto [kind, pi] = FindParentRef(lights_[i], idx);
            n.SetAttribute("parentKind", kind);
            n.SetAttribute("parentIndex", pi);
            lights_[i]->Serialize(n);
        }
    }

    void Scene::Deserialize(const XMLNode &node)
    {
        XMLNode root = node;

        // version: absent (-1) = blank/new file, otherwise the file was written by Serialize().
        int version = -1;
        root.GetAttribute("version", version);
        if (version > kSceneVersion)
        {
            // Written by a newer build; refuse without touching the live scene.
            lastLoadOk_ = false;
            return;
        }
        lastLoadOk_ = true;

        // OpenScene() deserializes into the *live* scene (not via SceneManager::SetScene),
        // so the previous contents must be cleared here.
        objects_.clear();
        lights_.clear();
        entities.Clear();
        selectedObj_   = -1;
        selectedLight_ = -1;

        std::string sceneName;
        root.GetAttribute("name", sceneName);
        if (!sceneName.empty())
            name = sceneName;

        struct PendingParent
        {
            Entity *entity;
            int kind;
            int index;
        };
        std::vector<PendingParent> pending;

        for (XMLNode n: root.GetChildren("Object"))
        {
            SceneObject *obj = entities.CreateEntity<SceneObject>(std::string()); // name set by Deserialize below
            obj->Deserialize(n);
            if (initialized_) // GPU resources only once the renderer exists; Initialize() handles the rest
                ResolveBuiltinMesh(obj);

            int parentKind = 0, parentIndex = -1;
            n.GetAttribute("parentKind", parentKind);
            n.GetAttribute("parentIndex", parentIndex);
            pending.push_back({obj, parentKind, parentIndex});

            objects_.push_back(obj);
        }

        for (XMLNode n: root.GetChildren("Light"))
        {
            SceneLight *light = entities.CreateEntity<SceneLight>(std::string());
            light->Deserialize(n);

            int parentKind = 0, parentIndex = -1;
            n.GetAttribute("parentKind", parentKind);
            n.GetAttribute("parentIndex", parentIndex);
            pending.push_back({light, parentKind, parentIndex});

            lights_.push_back(light);
        }

        // Reparent in a second pass, once every Entity in this batch exists;
        // an object can reference a parent that's deserialized later in the file.
        for (auto &p: pending)
        {
            Entity *parent = nullptr;
            if (p.kind == 1 && p.index >= 0 && p.index < (int) objects_.size())
                parent = objects_[p.index];
            else if (p.kind == 2 && p.index >= 0 && p.index < (int) lights_.size())
                parent = lights_[p.index];

            // Skip self-parenting and cycles from corrupt files.
            if (parent && !IsAncestorOrSelf(p.entity, parent))
                entities.GetRegistry().Reparent(p.entity, parent);
        }

        // Only a file with no version attribute (blank / brand new) gets defaults.
        // A saved scene that was deliberately emptied stays empty.
        if (version < 0 && objects_.empty() && lights_.empty())
            PopulateDefaults();

        SyncLightTransforms();
        RebuildLightManager();
    }

    bool Scene::SaveXML(const std::string &filename)
    {
        namespace fs = std::filesystem;

        auto *xml = XMLModule::Get();
        if (!xml)
            return false;

        // Write to a temp file and rename so a crash/failed write can't corrupt the existing scene.
        const fs::path target(filename);
        fs::path tmp = target;
        tmp += ".tmp";

        std::error_code ec;
        if (!xml->Serialize("Scene", *this, tmp.string()))
        {
            fs::remove(tmp, ec);
            return false;
        }

        fs::rename(tmp, target, ec); // replaces an existing target
        if (ec)
        {
            std::error_code ec2;
            fs::remove(tmp, ec2);
            return false;
        }
        return true;
    }

    bool Scene::ReadXML(const std::string &filename)
    {
        auto *xml = XMLModule::Get();
        if (!xml)
            return false;

        lastLoadOk_   = true;
        const bool ok = xml->Deserialize(filename, *this);
        return ok && lastLoadOk_;
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
