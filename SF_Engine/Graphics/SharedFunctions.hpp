#pragma once
#include <Math/BasicMath.hpp>
#include <string>

namespace SF::Engine
{
    // too many :(
    struct ApplicationTime;
    class ImageDepth;
    class Image2d;
    struct CameraUBO;
    class DescriptorSet;

    Vec3 GetMainDirectionalLightDirection();
    float GetMainDirectionalLightIntensity();
    glm::vec2 GetScreenSize();
    Mat4 GetView();
    Mat4 GetInvView();
    Mat4 GetProjection();
    Mat4 GetInvProjection();
    Mat4 GetPrevViewProjection();
    float GetFarPlane();
    float GetNearPlane();
    float GetFOV();
    Vec4 GetCameraDirection();
    Vec3 GetCameraPosition();
    Vec4 GetCameraPosition4();
    ApplicationTime GetDeltaTime();
    double GetDeltaTimeMilliS();
    int64_t GetDeltaTimeMicroS();
    int64_t GetDeltaTimeNanoS();

    const Image2d *GetSceneHDR();
    const ImageDepth *GetSceneDepth(
        const std::string &attachmentName = "gbuf_depth");

    void BindSharedCameraData(int bind, int count, DescriptorSet* set);
}