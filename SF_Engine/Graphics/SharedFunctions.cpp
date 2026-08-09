#include "SharedFunctions.hpp"

#include <Controllers/CameraController.hpp>
#include <Engine/Engine.hpp>

#include <Graphics/Windows/WindowManager.hpp>
#include <Graphics/Lighting/LightManager.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>

#include <Scene/SceneManager.hpp>
#include <Math/Time/Time.hpp>
#include <Math/BasicMath.hpp>


namespace SF::Engine
{
    Vec3 GetMainDirectionalLightDirection()
    {
        if (SceneManager::Get()->IsSceneStarted())
        {
            auto lights = SceneManager::Get()->GetScene()->GetAllLights(
                SceneManager::Get()->GetScene());

            if (lights.empty())
                return Vec3(0.0f);

            return normalize(lights[0].light.direction);
        }
        else
        {
            return Vec3(0, 1, 0);
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

    // TODO: Get current window
    Vec2 GetScreenSize()
    {
        return Vec2(WindowManager::Get()->GetWindow(0)->GetSize().x, WindowManager::Get()->GetWindow(0)->GetSize().y);
    }

    Mat4 GetView()
    {
        return CameraController::Get().GetActive()->GetView();
    }
    Mat4 GetInvView()
    {
        return inverse(CameraController::Get().GetActive()->GetView());
    }

    Mat4 GetProjection()
    {
        return CameraController::Get().GetActive()->GetProjection(WindowManager::Get()->GetWindow(0)->GetAspectRatio());
    }

    Mat4 GetInvProjection()
    {
        return inverse(GetProjection());
    }

    Mat4 GetPrevViewProjection()
    {
        return CameraController::Get().GetActive()->GetPrevViewProjection();
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

    Vec4 GetCameraDirection()
    {
        return Vec4(CameraController::Get().GetActive()->GetFront(), CameraController::Get().GetActive()->GetFarPlane());
    }

    Vec3 GetCameraPosition()
    {
        return CameraController::Get().GetActive()->GetPosition();
    }
    Vec4 GetCameraPosition4()
    {
        return Vec4(CameraController::Get().GetActive()->GetPosition(), 1.0f);
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
        // get attachment is a descriptor
        return dynamic_cast<const ImageDepth *>(rs->GetAttachment(attachmentName));
    }

    void BindSharedCameraData(int bind, int count, DescriptorSet* set)
    {
        VkDescriptorBufferInfo bi{GetSharedCameraBuffer().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w0{};
        w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet = set->GetDescriptorSet();
        w0.dstBinding = bind;
        w0.descriptorCount = count;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo = &bi;
        DescriptorSet::Update({w0});
    }

    const Image2d* GetGBufferAlbedo()
    {
        return dynamic_cast<const Image2d*>(RenderSystem::Get()->GetAttachment("gbuf_albedo"));
    }
}