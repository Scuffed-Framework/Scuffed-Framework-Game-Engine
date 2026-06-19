#pragma once

#include <memory>
#include <Graphics/Images/Image3d.hpp>
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>

namespace SF::Engine
{
    class AlligatorNoiseLUT
    {
    public:
        explicit AlligatorNoiseLUT(uint32_t size = 128);
        ~AlligatorNoiseLUT() = default;

        Image3d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image3d> texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        uint32_t size_;
        bool baked_ = false;
    };

} // namespace SF::Engine
