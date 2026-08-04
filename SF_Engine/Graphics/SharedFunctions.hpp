#pragma once
#include <glm/glm.hpp>
#include <string>

namespace SF::Engine
{
    // too many :(
    struct ApplicationTime;
    class ImageDepth;
    class Image2d;
    struct CameraUBO;
    class DescriptorSet;

    glm::vec3 GetMainDirectionalLightDirection();
    float GetMainDirectionalLightIntensity();
    glm::vec2 GetScreenSize();
    glm::mat4 GetView();
    glm::mat4 GetInvView();
    glm::mat4 GetProjection();
    glm::mat4 GetInvProjection();
    float GetFarPlane();
    float GetNearPlane();
    float GetFOV();
    glm::vec4 GetCameraDirection();
    glm::vec3 GetCameraPosition();
    glm::vec4 GetCameraPosition4();
    ApplicationTime GetDeltaTime();
    double GetDeltaTimeMilliS();
    int64_t GetDeltaTimeMicroS();
    int64_t GetDeltaTimeNanoS();

    const Image2d *GetSceneHDR();
    const ImageDepth *GetSceneDepth(
        const std::string &attachmentName = "gbuf_depth");

    void BindSharedCameraData(int bind, int count, DescriptorSet* set);
}