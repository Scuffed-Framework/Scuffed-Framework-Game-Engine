#pragma once

#include <Camera/Camera.hpp>
#include <LowLevel/Rocket.hpp>
#include "EntityHolder.hpp"
#include "SystemHolder.hpp"

#include <Graphics/Mesh/Mesh.hpp>
#include <Components/TransformComponent.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/AtmospherePipelinePass.hpp>

#include <XML/XMLReader.hpp>
#include "SceneSerialization.hpp"
#include "EngineUI.hpp"
#include <Scene/Types.hpp>
#include <Scene/SceneRenderer.hpp>
#include <Graphics/Visuals/sfSkies/Clouds/CloudPipelinePass.hpp>

#include <Graphics/Images/Image2d.hpp>
#include <Controllers/CameraController.hpp>

namespace SF::Engine
{
    class Scene : public virtual rocket::trackable, public Serializable
    {
        friend class SceneManager;

    public:
        explicit Scene(std::unique_ptr<CameraController> &&cameraController, std::string name, SceneRendererConfig cfg = {});
        virtual ~Scene() = default;

        virtual void Start() = 0;
        virtual void Update();
        virtual void Render();

        template <typename T>
        bool HasSystem() const { return systems.Has<T>(); }
        template <typename T>
        T *GetSystem() const { return systems.Get<T>(); }
        template <typename T, typename... Args>
        void AddSystem(Args &&...args)
        {
            systems.Add<T>(std::make_unique<T>(std::forward<Args>(args)...));
        }
        template <typename T>
        void RemoveSystem() { systems.Remove<T>(); }
        void ClearSystems();

        Entity GetEntity(const std::string &name) const;
        Entity CreateEntity();
        Entity CreatePrefabEntity(const std::string &filename);
        std::vector<Entity> QueryAllEntities();

        template <typename T>
        T *GetComponent(bool allowDisabled = false)
        {
            return entities.GetComponent<T>(allowDisabled);
        }

        template <typename T>
        std::vector<T *> QueryComponents(bool allowDisabled = false)
        {
            return entities.QueryComponents<T>(allowDisabled);
        }

        void ClearEntities();

        CameraController *GetCamera() const { return cameraController_.get(); }
        void SetCamera(std::unique_ptr<CameraController> c) { cameraController_ = std::move(c); }

        virtual bool IsPaused() const = 0;

        LightManager *GetLightManager() { return lightManager_.get(); }
        SceneRenderer *GetRenderer() const { return sceneRenderer_; }

        template <typename T, typename... Args>
        T *InjectPipelinePass(const Pipeline::Stage &stage, Args &&...args)
        {
            assert(sceneRenderer_ && "InjectPipelinePass called before renderer is ready");
            return sceneRenderer_->AddPipelinePass<T>(stage, std::forward<Args>(args)...);
        }

        template <typename T>
        T *GetPipelinePass() const
        {
            if (!sceneRenderer_)
                return nullptr;
            return sceneRenderer_->GetPipelinePass<T>();
        }

        void SaveXML(const std::string &filename = "scene.xml");
        void ReadXML(const std::string &filename = "scene.xml");

        void Initialize();

        void Serialize(XMLNode &node) const override;
        void Deserialize(const XMLNode &node) override;

        void Stop()
        {
            started_ = false;
            initialized_ = false;
            ClearSystems();
            ClearEntities();
            objects_.clear();
            lights_.clear();
        }

        static std::vector<SceneLight> GetAllLights(Scene *scene)
        {
            return scene->lights_;
        }

    protected:
        // Subclasses can set these before Initialize() to opt into features.
        bool sunEnabled = false;
        bool atmosphereEnabled = false;

    private:
        bool started_ = false;
        bool initialized_ = false;

        SceneRendererConfig rendererCfg_;
        SceneRenderer *sceneRenderer_ = nullptr;

        SystemHolder systems;
        EntityHolder entities;

        std::unique_ptr<CameraController> cameraController_;

        std::shared_ptr<LightManager> lightManager_;
        LitMeshPipelinePass *litPass_ = nullptr;
        AtmospherePipelinePass *atmoPass_ = nullptr;
        CloudPipelinePass *cloudPass_ = nullptr;

        std::vector<SceneObject> objects_;
        std::vector<SceneLight> lights_;

        float elapsed_ = 0.0f;
        uint32_t frameIndex_ = 0;

        glm::mat4 prevViewProj_ = glm::mat4(1.0f);

        uint32_t lastScreenW_ = 0;
        uint32_t lastScreenH_ = 0;

        std::chrono::steady_clock::time_point lastFrameTime_;

        EngineUI ui_;
        int selectedObj_ = -1;
        int selectedLight_ = -1;

        void SyncLightTransforms()
        {
            for (auto &sl : lights_)
            {
                sl.light.position = sl.transform.position;
                if (sl.light.type == Lighting::LightType::Directional)
                {
                    glm::vec3 rot = glm::radians(sl.transform.rotation);
                    glm::mat4 m = glm::rotate(glm::mat4(1.0f), rot.y, {0, 1, 0});
                    m = glm::rotate(m, rot.x, {1, 0, 0});
                    m = glm::rotate(m, rot.z, {0, 0, 1});
                    sl.light.direction = glm::normalize(glm::vec3(m * glm::vec4(0, -1, 0, 0)));
                }
            }
        }

        void RebuildLightManager()
        {
            if (!lightManager_)
                return;
            lightManager_->ClearLights();
            for (auto &sl : lights_)
                lightManager_->AddLight(sl.light);
        }

        std::string name;

    public:
        const std::string GetName() { return name; }
        static const ImageDepth *GetDepthTexture();
    };
}
