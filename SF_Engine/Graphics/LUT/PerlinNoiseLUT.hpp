#pragma once

#include <memory>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Images/Image3d.hpp>
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>

namespace SF::Engine
{
    class PerlinNoiseLUT
    {
    public:
        explicit PerlinNoiseLUT(uint32_t size = 128);
        ~PerlinNoiseLUT() = default;

        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image2d> texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        uint32_t size_;
        bool baked_ = false;
    };

} // namespace SF::Engine
