#pragma once
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <memory>

namespace SF::Engine
{
    class MultiScatterLUT
    {
    public:
        // transmittanceLUT must already be baked and in SHADER_READ_ONLY_OPTIMAL
        explicit MultiScatterLUT(Image2d *transmittanceLUT,
                                 uint32_t width = 32,
                                 uint32_t height = 32);

        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        std::unique_ptr<Image2d> texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        bool baked_ = false;
    };
}