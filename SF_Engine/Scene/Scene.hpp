#pragma once

#include <3rdPartyLibs/Rocket.hpp>
#include <EntityComponentSystem/EntityHolder.hpp>
#include <Rendering/Camera/Camera.hpp>
#include "SystemHolder.hpp"

#include <Rendering/Lighting/LitMeshSFERenderpass.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/Visuals/Sky/Atmosphere/AtmosphereSFERenderpass.hpp>

#include <LowLevel/XML/XMLModule.hpp>
#include <Rendering/Visuals/Sky/Clouds/CloudSFERenderpass.hpp>
#include <Scene/SceneRenderer.hpp>
#include <Scene/Types.hpp>
#include "SceneSerialization.hpp"
#include <Assets/AssetPipeline.hpp>

#include <Controllers/CameraController.hpp>
#include <Rendering/RHI/Images/Image.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    // todo: can we move some boilerplate out of this class?
    class Scene : public virtual rocket::trackable, public Serializable
    {
        friend class SceneManager;
        friend class SceneRenderer;

    public:
        static constexpr int kSceneVersion = 2;

        // populateDefaults = false lets subclasses start from an empty scene instead of
        // getting the sun / point light / cube / floor.
        explicit Scene(::std::unique_ptr<CameraController> &&cameraController, std::string name,
                       const SceneRendererConfig &cfg = {}, bool populateDefaults = true);
        virtual ~Scene();

        virtual void Start() = 0;
        virtual void Update();
        virtual void Render();

        template<typename T>
        bool HasSystem() const
        {
            return systems.Has<T>();
        }
        template<typename T>
        T *GetSystem() const
        {
            return systems.Get<T>();
        }
        template<typename T, typename... Args>
        void AddSystem(Args &&...args)
        {
            systems.Add<T>(std::make_unique<T>(std::forward<Args>(args)...));
        }
        template<typename T>
        void RemoveSystem()
        {
            systems.Remove<T>();
        }
        void ClearSystems();

        Entity *GetEntity(const std::string &name) const;
        Entity *GetEntity(const EntityId &id);
        Entity *CreateEntity();
        EntityHolder *GetEntities() { return &entities; }
        std::vector<Entity *> QueryAllEntities();

        // Also purges SceneObject / SceneLight caches for the entity and its whole subtree.
        void RemoveEntity(Entity *entity);

        void AddEntity(const std::string &name, Entity *parent = nullptr)
        {
            if (!parent)
                entities.CreateEntity(name);
            else
                entities.CreateChildEntity(parent, name);
        }
        void ReparentEntity(EntityId childId, EntityId newParentId)
        {
            auto *child     = entities.FindById(childId);
            auto *newParent = entities.FindById(newParentId);
            if (!child || !newParent || child == newParent)
                return;
            if (IsAncestorOrSelf(child, newParent)) // would create a cycle
                return;
            entities.Reparent(child, newParent);
        }

        template<typename T>
        T *GetComponent(bool allowDisabled = false)
        {
            return entities.GetComponent<T>(allowDisabled);
        }

        template<typename T>
        std::vector<T *> QueryComponents(bool allowDisabled = false)
        {
            return entities.QueryComponents<T>(allowDisabled);
        }

        void ClearEntities();

        CameraController *GetCamera() const { return cameraController_.get(); }
        void SetCamera(std::unique_ptr<CameraController> c) { cameraController_ = std::move(c); }

        virtual bool IsPaused() const = 0;

        LightManager *GetLightManager() { return sceneRenderer_ ? sceneRenderer_->GetLightManager() : nullptr; }
        SceneRenderer *GetRenderer() const { return sceneRenderer_; }

        template<typename T, typename... Args>
        T *InjectSFERenderpass(const Pipeline::Stage &stage, Args &&...args)
        {
            assert(sceneRenderer_ && "InjectSFERenderpass called before renderer is ready");
            return sceneRenderer_->AddSFERenderpass<T>(stage, std::forward<Args>(args)...);
        }

        template<typename T>
        T *GetSFERenderpass() const
        {
            if (!sceneRenderer_)
                return nullptr;
            return GetRenderer()->GetSFERenderpass<T>();
        }

        // Atomic save (temp file + rename).
        bool SaveXML(const std::string &filename = "scene.xml");
        // Returns false if the file couldn't be parsed or was written by a newer version.
        // Prefer OpenScene() (deferred, safe mid-frame) over calling this directly on a live scene.
        bool ReadXML(const std::string &filename = "scene.xml");

        // File this scene was loaded from / saved to. Empty if none. Lives and dies with the scene.
        const std::filesystem::path &GetPath() const { return scenePath_; }
        void SetPath(std::filesystem::path p) { scenePath_ = std::move(p); }
        void ClearPath() { scenePath_.clear(); }

        void Initialize();

        void Serialize(XMLNode &node) const override;
        void Deserialize(const XMLNode &node) override;

        void Stop()
        {
            started_     = false;
            initialized_ = false;
            ClearSystems();
            ClearEntities();
            objects_.clear();
            lights_.clear();
            selectedObj_   = -1;
            selectedLight_ = -1;
            // The renderer is owned by RenderSystem and gets replaced by the next scene;
            // don't keep dangling pointers to it or its passes.
            sceneRenderer_ = nullptr;
            litPass_       = nullptr;
            atmoPass_      = nullptr;
            cloudPass_     = nullptr;
        }

    protected:
        // Subclasses can set these before Initialize() to opt out of / into features.
        // atmosphereEnabled defaults to true to preserve the previous always-on behaviour.
        // sunEnabled is currently reserved (not read by Scene).
        bool sunEnabled        = false;
        bool atmosphereEnabled = true;

    private:
        bool started_     = false;
        bool initialized_ = false;
        bool lastLoadOk_  = true;

        SceneRendererConfig rendererCfg_;
        SceneRenderer *sceneRenderer_ = nullptr;

        SystemHolder systems;
        EntityHolder entities;

        std::unique_ptr<CameraController> cameraController_;

        LitMeshSFERenderpass *litPass_     = nullptr;
        AtmosphereSFERenderpass *atmoPass_ = nullptr;
        CloudSFERenderpass *cloudPass_     = nullptr;

        float elapsed_       = 0.0f;
        uint32_t frameIndex_ = 0;

        Mat4 prevViewProj_ = Mat4(1.0f);

        uint32_t lastScreenW_ = 0;
        uint32_t lastScreenH_ = 0;

        std::chrono::steady_clock::time_point lastFrameTime_;

        int selectedObj_   = -1;
        int selectedLight_ = -1;

        std::filesystem::path scenePath_;

        // NOTE: uses the local Transform::position / rotation. For parented lights this is
        // wrong if Transform is local-space; swap in the world-space accessor if you have one.
        void SyncLightTransforms()
        {
            for (auto &sl: lights_)
            {
                sl->GetComponent<Light>()->position = sl->GetComponent<Transform>()->position;
                if (sl->GetComponent<Light>()->type == Lighting::LightType::Directional)
                {
                    Vec3 rot                             = glm::radians(sl->GetComponent<Transform>()->rotation);
                    Mat4 m                               = glm::rotate(Mat4(1.0f), rot.y, {0, 1, 0});
                    m                                    = glm::rotate(m, rot.x, {1, 0, 0});
                    m                                    = glm::rotate(m, rot.z, {0, 0, 1});
                    sl->GetComponent<Light>()->direction = normalize(Vec3(m * Vec4(0, -1, 0, 0)));
                }
            }
        }

        void RebuildLightManager()
        {
            if (!GetLightManager())
                return;
            GetLightManager()->ClearLights();
            for (auto &sl: lights_)
                GetLightManager()->AddLight(*sl->GetComponent<Light>());
        }

        void PopulateDefaults(); // sun, point light, cube, floor
        static void ResolveBuiltinMesh(SceneObject *obj);
        static void ResolveAssetRefs(SceneObject *obj); // meshAsset / albedoAsset / audioAsset -> live resources

        static bool IsAncestorOrSelf(const Entity *anc, const Entity *e)
        {
            for (; e; e = e->GetParent())
                if (e == anc)
                    return true;
            return false;
        }

        std::string name;

    public:
        std::string GetName() const { return name; }
        static const ImageDepth *GetDepthTexture();

        SceneObject *AddObject(const std::string &name, Entity *parent = nullptr);
        SceneObject *AddObject(const std::string &name, Transform &transform, Entity *parent = nullptr);
        SceneLight *AddLight(const std::string &name, Lighting::LightType type, const Vec3 &color, float intensity,
                             const Vec3 &position, const Vec3 &rotation, Entity *parent = nullptr);

        SceneLight *AddLight(const std::string &name, Lighting::LightType type, const Vec3 &color, float intensity,
                             Transform &transform, Entity *parent = nullptr);

        void RemoveObject(SceneObject *obj);
        void RemoveLight(SceneLight *light);

        /**
         * Places an imported asset in the scene at `position` (world units).
         *  - MeshAsset : one SceneObject, or an empty SceneObject group with one child per part (multi-material).
         *  - ImageAsset: a textured quad standing on `position`, aspect-correct, 1 unit tall.
         *  - AudioAsset: an emitter SceneObject carrying an AudioClip (not auto-played).
         * Returns the new (root) object, or nullptr if the asset can't be placed (type unsupported / load failed).
         */
        SceneObject *InstantiateAsset(const std::shared_ptr<AssetBase> &asset, const Vec3 &position,
                                      Entity *parent = nullptr);

        const std::vector<SceneObject *> &GetObjects() const { return objects_; }
        const std::vector<SceneLight *> &GetLights() const { return lights_; }

        static std::vector<SceneLight *> GetAllLights(Scene *scene) { return scene->lights_; }

    private:
        // Non-owning flat caches for fast iteration (rendering, serialization,
        // hierarchy UI). Real ownership lives in the Entity tree inside
        // `entities` (EntityHolder -> EntityRegistry -> roots/children).
        std::vector<SceneObject *> objects_;
        std::vector<SceneLight *> lights_;

        // entity -> {kind, index}; kind: 1 = objects_[index], 2 = lights_[index]
        using IndexMap = std::unordered_map<const Entity *, std::pair<int, int>>;

        // Returns the nearest SceneObject/SceneLight ancestor (plain Entity ancestors are skipped,
        // since they are not serialized).
        // kind: 0 = root, 1 = objects_[index] is the parent, 2 = lights_[index] is the parent
        static std::pair<int, int> FindParentRef(Entity *e, const IndexMap &idx);

        void PurgeFromCaches(Entity *rootEntity);
        void RemoveEntitySubtree(Entity *rootEntity);
    };

    inline constexpr string_view SceneFileExtension = ".scene";

} // namespace SF::Engine
