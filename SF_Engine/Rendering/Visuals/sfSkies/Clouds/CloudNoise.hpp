#pragma once
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Rendering/Images/Image3d.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <memory>

namespace SF::Engine
{
    struct WeatherParams
    {
        uint32_t resolution   = 1024;
        float    coverageScale = 4.0f;
        float    heightScale   = 1.0f;   // << coverageScale; height varies more slowly
        float    typeScale     = 0.25f;  // << heightScale; whole fronts share a type
        float    coverageBias  = 0.0f;
        float    _pad[3]{};              // std140 cbuffer alignment
    };

    class CloudNoiseLUTs
    {
    public:
        explicit CloudNoiseLUTs(uint32_t width = 32, uint32_t height = 32, uint32_t depth = 1)
        {
            BaseNoiseTexture_ = std::make_unique<Image3d>(
                UVec3{width, height, depth},
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            DetailNoiseTexture_ = std::make_unique<Image3d>(
                UVec3{width, height, depth},
                VK_FORMAT_R8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            CurlNoiseTexture_ = std::make_unique<Image2d>(
                UVec2{width, height},
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            WeatherTexture_ = std::make_unique<Image2d>(
                UVec2{params_.resolution, params_.resolution},
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);


            uboD_ = std::make_unique<UniformBuffer>(sizeof(WeatherParams));
            uboD_->Update(params_);

            pipelineA_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/BaseNoise.shader");

            pipelineB_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/DetailNoise.shader");

            pipelineC_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/CurlNoise.shader");

            pipelineD_ = std::make_unique<ComputePipeline>(
                "Shaders/Clouds/WeatherMap.shader");

            descSetA_ = std::make_unique<DescriptorSet>(*pipelineA_);
            descSetB_ = std::make_unique<DescriptorSet>(*pipelineB_);
            descSetC_ = std::make_unique<DescriptorSet>(*pipelineC_);
            descSetD_ = std::make_unique<DescriptorSet>(*pipelineD_);

            auto info0 = BaseNoiseTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);
            auto info1 = DetailNoiseTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);
            auto info2 = CurlNoiseTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);
            auto info3 = WeatherTexture_->GetWriteDescriptor(
                0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);

            // Patch dstSet  GetWriteDescriptor leaves it null
            VkWriteDescriptorSet w0 = info0.GetWriteDescriptorSet();
            w0.dstSet = descSetA_->GetDescriptorSet();

            VkWriteDescriptorSet w1 = info1.GetWriteDescriptorSet();
            w1.dstSet = descSetB_->GetDescriptorSet();

            VkWriteDescriptorSet w2 = info2.GetWriteDescriptorSet();
            w2.dstSet = descSetC_->GetDescriptorSet();

            VkWriteDescriptorSet w3 = info3.GetWriteDescriptorSet();
            w3.dstSet = descSetD_->GetDescriptorSet();
            
            VkDescriptorBufferInfo bufInfoA{uboD_->GetBuffer(), 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet b1{};
            b1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            b1.dstSet = descSetD_->GetDescriptorSet();
            b1.dstBinding = 1;
            b1.descriptorCount = 1;
            b1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b1.pBufferInfo = &bufInfoA;

            DescriptorSet::Update({w0, w1, w2, w3, b1});
        }

        Image3d *GetBaseTexture() const { return BaseNoiseTexture_.get(); }
        Image3d *GetDetailTexture() const { return DetailNoiseTexture_.get(); }
        Image2d *GetCurlTexture() const { return CurlNoiseTexture_.get(); }
        Image2d *GetWeatherTexture() const { return WeatherTexture_.get(); }

        void Bake(const CommandBuffer &cmd)
        {
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
                Image::InsertImageMemoryBarrier(
                    cmd, CurlNoiseTexture_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

                Image::InsertImageMemoryBarrier(
                    cmd, WeatherTexture_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            }
            // uboD_->Update(params_); maybe?

            auto ext0 = BaseNoiseTexture_->GetExtent();
            auto ext1 = DetailNoiseTexture_->GetExtent();
            auto ext2 = CurlNoiseTexture_->GetExtent();
            auto ext3 = WeatherTexture_->GetExtent();

            pipelineA_->BindPipeline(cmd);
            descSetA_->BindDescriptor(cmd);
            vkCmdDispatch(cmd, (ext0.x + 7) / 8, (ext0.y + 7) / 8, ext0.z);

            pipelineB_->BindPipeline(cmd);
            descSetB_->BindDescriptor(cmd);
            vkCmdDispatch(cmd, (ext1.x + 7) / 8, (ext1.y + 7) / 8, ext1.z);

            pipelineC_->BindPipeline(cmd);
            descSetC_->BindDescriptor(cmd);
            vkCmdDispatch(cmd, (ext2.x + 7) / 8, (ext2.y + 7) / 8, 1);

            pipelineD_->BindPipeline(cmd);
            descSetD_->BindDescriptor(cmd);
            vkCmdDispatch(cmd, (ext3.x + 7) / 8, (ext3.y + 7) / 8, 1);

            Image::InsertImageMemoryBarrier(
                cmd, BaseNoiseTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd, DetailNoiseTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd, CurlNoiseTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd, WeatherTexture_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            baked_ = true;
            BaseNoiseTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            DetailNoiseTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            CurlNoiseTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            WeatherTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

    private:
        ::std::unique_ptr<Image3d> BaseNoiseTexture_;
        ::std::unique_ptr<Image3d> DetailNoiseTexture_;
        ::std::unique_ptr<Image2d> CurlNoiseTexture_;
        ::std::unique_ptr<Image2d> WeatherTexture_;

        ::std::unique_ptr<ComputePipeline> pipelineA_;
        ::std::unique_ptr<ComputePipeline> pipelineB_;
        ::std::unique_ptr<ComputePipeline> pipelineC_;
        ::std::unique_ptr<ComputePipeline> pipelineD_;
        
        ::std::unique_ptr<DescriptorSet> descSetA_;
        ::std::unique_ptr<DescriptorSet> descSetB_;
        ::std::unique_ptr<DescriptorSet> descSetC_;
        ::std::unique_ptr<DescriptorSet> descSetD_;

        WeatherParams params_;
        ::std::unique_ptr<UniformBuffer> uboD_;
        bool baked_ = false;
    };
}