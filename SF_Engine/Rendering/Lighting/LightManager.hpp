#pragma once

#include "LightingTypes.hpp"
#include "Light.hpp"
#include <Rendering/Buffers/StorageBuffer.hpp>
#include <Rendering/Buffers/UniformBuffer.hpp>
#include <vector>
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Owns all GPU lighting buffers and the CPU-side light list.
     *
     * Holds:
     *   - frameUBO        : camera/frame data (set=0 bind=0)
     *   - lightSSBO       : GpuLight array   (set=0 bind=1)
     *   - clusterSSBO     : GpuCluster AABBs (set=0 bind=2)
     *   - lightListSSBO   : GpuClusterLightList headers (set=0 bind=3)
     *   - lightIndexSSBO  : flat uint indices (set=0 bind=4)
     *
     * Call Upload() once per frame after updating lights / camera.
     */
    class LightManager
    {
    public:
        LightManager();
        ~LightManager() = default;

        //  Light list management
        void AddLight(const Light &light);
        void RemoveLight(uint32_t index);
        void ClearLights();
        // Note: GetLight returns GpuLight since that is what is stored internally.
        // The Light (with string name) is only in the scene's lights_ vector.
        Lighting::GpuLight &GetLight(uint32_t index) { return cpuLights_[index]; }
        const Lighting::GpuLight &GetLight(uint32_t index) const { return cpuLights_[index]; }
        uint32_t GetLightCount() const { return static_cast<uint32_t>(cpuLights_.size()); }

        //  Per-frame upload
        /**
         * @brief Upload frame constants and light list to GPU.
         * @param frame   Filled GpuFrameData (camera matrices etc.)
         */
        void Upload(const Lighting::GpuFrameData &frame);

        //  Buffer accessors (bind into descriptor sets)
        const UniformBuffer &GetFrameUBO() const { return *frameUBO_; }
        const StorageBuffer &GetLightSSBO() const { return *lightSSBO_; }
        const StorageBuffer &GetClusterSSBO() const { return *clusterSSBO_; }
        const StorageBuffer &GetLightListSSBO() const { return *lightListSSBO_; }
        const StorageBuffer &GetLightIndexSSBO() const { return *lightIndexSSBO_; }

    private:
        std::vector<Lighting::GpuLight> cpuLights_; // stores GPU-ready lights, no std::string

        std::unique_ptr<UniformBuffer> frameUBO_;
        std::unique_ptr<StorageBuffer> lightSSBO_;
        std::unique_ptr<StorageBuffer> clusterSSBO_;
        std::unique_ptr<StorageBuffer> lightListSSBO_;
        std::unique_ptr<StorageBuffer> lightIndexSSBO_;
    };
}
