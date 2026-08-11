#include "LightManager.hpp"

#include <algorithm>
#include <stdexcept>

namespace SF::Engine
{
    LightManager::LightManager()
    {
        using namespace Lighting;

        frameUBO_ = std::make_unique<UniformBuffer>(sizeof(GpuFrameData));

        lightSSBO_ = std::make_unique<StorageBuffer>(
            sizeof(GpuLight) * MAX_LIGHTS);

        clusterSSBO_ = std::make_unique<StorageBuffer>(
            sizeof(GpuCluster) * CLUSTER_COUNT);

        lightListSSBO_ = std::make_unique<StorageBuffer>(
            sizeof(GpuClusterLightList) * CLUSTER_COUNT);

        // worst-case: every light visible in every cluster
        lightIndexSSBO_ = std::make_unique<StorageBuffer>(
            sizeof(uint32_t) * CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER);

        // Pre-reserve to avoid reallocation-induced moves of Light objects
        // (which contain std::string name that can corrupt under moves).
        cpuLights_.reserve(64);
    }

    void LightManager::AddLight(const Light &light)
    {
        if (cpuLights_.size() >= Lighting::MAX_LIGHTS)
            throw std::runtime_error("LightManager: MAX_LIGHTS exceeded");
        cpuLights_.push_back(light.ToGpu());
    }

    void LightManager::RemoveLight(uint32_t index)
    {
        if (index < cpuLights_.size())
            cpuLights_.erase(cpuLights_.begin() + index);
    }

    void LightManager::ClearLights()
    {
        cpuLights_.clear();
    }

    void LightManager::Upload(const Lighting::GpuFrameData &frame)
    {
        // Upload frame constants
        frameUBO_->Update(frame);

        // cpuLights_ already stores GpuLight  upload directly, no conversion needed
        if (!cpuLights_.empty())
            lightSSBO_->Update(std::span<const Lighting::GpuLight>(cpuLights_));
    }
}
