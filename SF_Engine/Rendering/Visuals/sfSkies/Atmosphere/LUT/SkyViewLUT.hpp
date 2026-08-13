#pragma once
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <memory>
#include <Rendering/Buffers/UniformBuffer.hpp>
#include "../AtmosphereParams.hpp"

namespace SF::Engine
{
    struct SkyViewPushConstants
    {
        Vec4 sunDir;
        Vec4 camPos;
        float bottomRadius;
        float topRadius;
        Vec4 pad0_;
    };

    class SkyViewLUT
    {
    public:
        explicit SkyViewLUT(Image2d *transmittanceLUT, Image2d *multiScatterLUT,
                            uint32_t width = 128, uint32_t height = 128);
        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);
        void SetParams(const SkyViewPushConstants &p) { push_ = p; }

    private:
        std::unique_ptr<Image2d> texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_; // replaces push constants
        SkyViewPushConstants push_{};
        bool baked_ = false;
    };
}