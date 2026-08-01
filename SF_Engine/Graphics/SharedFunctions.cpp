#include "SharedFunctions.hpp"
#include <glm/glm.hpp>
#include <Graphics/Windows/WindowManager.hpp>
#include <Controllers/CameraController.hpp>
#include <Engine/Engine.hpp>
#include <Scene/SceneManager.hpp>
#include <Graphics/Lighting/LightManager.hpp>
#include <Math/Time/Time.hpp>

namespace SF::Engine
{
    glm::vec3 GetMainDirectionalLightDirection()
    {
        if (SceneManager::Get()->IsSceneStarted())
        {
            auto lights = SceneManager::Get()->GetScene()->GetAllLights(
                SceneManager::Get()->GetScene());

            if (lights.empty())
                return glm::vec3(0.0f);

            return glm::normalize(lights[0].light.direction);
        }
        else
        {
            return glm::vec3(0, 1, 0);
        }
    }

    float GetMainDirectionalLightIntensity()
    {
        auto lights = SceneManager::Get()->GetScene()->GetAllLights(
            SceneManager::Get()->GetScene());

        if (lights.empty())
            return 0.0f;

        return lights[0].light.intensity;
    }

    glm::vec2 GetScreenSize()
    {
        return glm::vec2(WindowManager::Get()->GetWindow(0)->GetSize().x, WindowManager::Get()->GetWindow(0)->GetSize().y);
    }

    glm::mat4 GetView()
    {
        return CameraController::Get().GetActive()->GetView();
    }
    glm::mat4 GetInvView()
    {
        return glm::inverse(CameraController::Get().GetActive()->GetView());
    }

    glm::mat4 GetProjection()
    {
        return CameraController::Get().GetActive()->GetProjection(WindowManager::Get()->GetWindow(0)->GetAspectRatio());
    }

    glm::mat4 GetInvProjection()
    {
        return glm::inverse(GetProjection());
    }

    float GetFarPlane()
    {
        return CameraController::Get().GetActive()->GetFarPlane();
    }

    float GetNearPlane()
    {
        return CameraController::Get().GetActive()->GetNearPlane();
    }

    float GetFOV()
    {
        return CameraController::Get().GetActive()->GetFieldOfView();
    }

    glm::vec4 GetCameraDirection()
    {
        return glm::vec4(CameraController::Get().GetActive()->GetFront(), CameraController::Get().GetActive()->GetFarPlane());
    }

    glm::vec3 GetCameraPosition()
    {
        return CameraController::Get().GetActive()->GetPosition();
    }
    glm::vec4 GetCameraPosition4()
    {
        return glm::vec4(CameraController::Get().GetActive()->GetPosition(), 1.0f);
    }

    ApplicationTime GetDeltaTime()
    {
        return Engine::Get()->GetDelta();
    }

    double GetDeltaTimeMilliS()
    {
        return Engine::Get()->GetDelta().AsMilliseconds();
    }
    int64_t GetDeltaTimeMicroS()
    {
        return Engine::Get()->GetDelta().AsMicroseconds();
    }
    int64_t GetDeltaTimeNanoS()
    {
        return Engine::Get()->GetDelta().AsNanoseconds();
    }

    const Image2d *GetSceneHDR()
    {
        auto *rs = RenderSystem::Get();
        return dynamic_cast<const Image2d *>(rs->GetAttachment("hdr"));
    }

    const ImageDepth *GetSceneDepth(const std::string &attachmentName)
    {
        auto *rs = RenderSystem::Get();
        return dynamic_cast<const ImageDepth *>(rs->GetAttachment(attachmentName));
    }
}