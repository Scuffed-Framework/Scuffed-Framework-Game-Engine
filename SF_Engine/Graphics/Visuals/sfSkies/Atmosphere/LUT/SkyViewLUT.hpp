#pragma once
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <memory>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include "../AtmosphereParams.hpp"

namespace SF::Engine
{
    struct SkyViewPushConstants
    {
        Vec4 sunDir; // .xyz = direction, .w = intensity
        float cameraHeight;
        float bottomRadius;
        float topRadius;
        float _pad;
        Vec4 cameraPos; // add: xyz = planet-relative metres, w = unused
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