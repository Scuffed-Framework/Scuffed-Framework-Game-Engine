#pragma once
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Images/Image3d.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <memory>

namespace SF::Engine
{
    class CloudNoiseLUTs
    {
    public:
        explicit CloudNoiseLUTs(uint32_t width = 32, uint32_t height = 32, uint32_t depth = 1)
        {
            BaseNoiseTexture_ = std::make_unique<Image3d>(
                VkExtent3D{width, height, depth},
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            DetailNoiseTexture_ = std::make_unique<Image3d>(
                VkExtent3D{width, height, depth},
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            pipelineA_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/BaseNoise.shader");

            pipelineB_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/DetailNoise.shader");

            descSetA_ = std::make_unique<DescriptorSet>(*pipelineA_);
            descSetB_ = std::make_unique<DescriptorSet>(*pipelineB_);

            auto info0 = BaseNoiseTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);
            auto info1 = DetailNoiseTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);

            // Patch dstSet  GetWriteDescriptor leaves it null
            VkWriteDescriptorSet w0 = info0.GetWriteDescriptorSet();
            w0.dstSet = descSetA_->GetDescriptorSet();

            VkWriteDescriptorSet w1 = info1.GetWriteDescriptorSet();
            w1.dstSet = descSetB_->GetDescriptorSet();

            DescriptorSet::Update({w0, w1});
        }

        Image3d *GetBaseTexture() const { return BaseNoiseTexture_.get(); }
        Image3d *GetDetailTexture() const { return DetailNoiseTexture_.get(); }
        
        void Bake(const CommandBuffer &cmd)
        {
            // On re-bake the image may already be SHADER_READ_ONLY_OPTIMAL; transition back.
            if (baked_)
            {

                Image::InsertImageMemoryBarrier(
                    cmd, BaseNoiseTexture_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                Image::InsertImageMemoryBarrier(
                    cmd, DetailNoiseTexture_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            }

            pipelineA_->BindPipeline(cmd);
            pipelineB_->BindPipeline(cmd);

            descSetA_->BindDescriptor(cmd);
            descSetB_->BindDescriptor(cmd);

            auto ext1 = BaseNoiseTexture_->GetExtent();
            auto ext2 = DetailNoiseTexture_->GetExtent();
            vkCmdDispatch(cmd, ext1.width, ext1.height, 1);
            vkCmdDispatch(cmd, ext2.width, ext2.height, 1);

            Image::InsertImageMemoryBarrier(
                cmd,
                BaseNoiseTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd,
                DetailNoiseTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1, 0, 1, 0);

            baked_ = true;
            BaseNoiseTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            DetailNoiseTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

    private:
        ::std::unique_ptr<Image3d> BaseNoiseTexture_;
        ::std::unique_ptr<Image3d> DetailNoiseTexture_;
        ::std::unique_ptr<ComputePipeline> pipelineA_;
        ::std::unique_ptr<ComputePipeline> pipelineB_;
        ::std::unique_ptr<DescriptorSet> descSetA_;
        ::std::unique_ptr<DescriptorSet> descSetB_;
        bool baked_ = false;
    };
}