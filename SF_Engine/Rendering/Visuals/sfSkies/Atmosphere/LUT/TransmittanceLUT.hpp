#pragma once
#include <Rendering/RHI/Commands/CommandBuffer.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image2d.hpp>
#include <Rendering/RHI/Pipelines/ComputePipeline.hpp>
#include <memory>

namespace SF::Engine
{
    class TransmittanceLUT
    {
    public:
        // 256×64 is the standard starter size
        explicit TransmittanceLUT(uint32_t width = 256, uint32_t height = 64);

        // Returns the baked texture  pass to AtmospherePipelinePass
        Image2d *GetTexture() const { return texture_.get(); }

        // Call once after construction (or when params change)
        void Bake(const CommandBuffer &cmd);

    private:
        std::unique_ptr<Image2d> texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        bool baked_ = false;
    };
}