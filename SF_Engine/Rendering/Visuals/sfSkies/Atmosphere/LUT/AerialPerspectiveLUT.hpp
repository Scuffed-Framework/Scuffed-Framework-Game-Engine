#pragma once
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Images/Image3d.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Buffers/UniformBuffer.hpp>
#include "../AtmosphereParams.hpp"
#include <memory>

#include <Rendering/SharedSamplers.hpp>

namespace SF::Engine
{
    //  aerialPerspColorRGBTransR_  rgba16f  3-D   .rgb = inscatter, .a = T.r
    //  aerialPerspTransGB_         rg16f    3-D   .rg  = T.gb
    //  aerialPerspRange_           r32f     2-D   per-XY-texel distToTravel
    //
    //  Call Bake() every frame from AtmospherePipelinePass::PreRender(), passing
    //  the current AtmosphereFrameUBO.  The bake is cheap: 4×4 workgroups ×
    //  32 slice iterations = negligible GPU time.
    class AerialPerspectiveLUT
    {
    public:
        explicit AerialPerspectiveLUT(Image2d *transmittanceLUT,
                                      Image2d *msLUT,
                                      uint32_t width = 32,
                                      uint32_t height = 32,
                                      uint32_t depth = 32)
            : width_(width), height_(height), depth_(depth)
        {
            ubo_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));

            aerialPerspColorRGBTransR_ = std::make_unique<Image3d>(
                UVec3{width_, height_, depth_},
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            aerialPerspTransGB_ = std::make_unique<Image3d>(
                UVec3{width_, height_, depth_},
                VK_FORMAT_R16G16_SFLOAT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            aerialPerspRange_ = std::make_unique<Image2d>(
                UVec2{width_, height_},
                VK_FORMAT_R32_SFLOAT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

            pipeline_ = std::make_unique<ComputePipeline>(
                "Shaders/Atmosphere/AerialPerspectiveLUT.shader");

            descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

            VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet w0{};
            w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w0.dstSet = descSet_->GetDescriptorSet();
            w0.dstBinding = 0;
            w0.descriptorCount = 1;
            w0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w0.pBufferInfo = &bi;

            VkDescriptorImageInfo II1{};
            II1.imageView = aerialPerspColorRGBTransR_->GetView();
            II1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet w1{};
            w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w1.dstSet = descSet_->GetDescriptorSet();
            w1.dstBinding = 1;
            w1.descriptorCount = 1;
            w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w1.pImageInfo = &II1;

            VkDescriptorImageInfo II2{};
            II2.imageView = aerialPerspTransGB_->GetView();
            II2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet w2{};
            w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w2.dstSet = descSet_->GetDescriptorSet();
            w2.dstBinding = 2;
            w2.descriptorCount = 1;
            w2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w2.pImageInfo = &II2;

            VkDescriptorImageInfo II3{};
            II3.imageView = aerialPerspRange_->GetView();
            II3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet w3{};
            w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w3.dstSet = descSet_->GetDescriptorSet();
            w3.dstBinding = 3;
            w3.descriptorCount = 1;
            w3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w3.pImageInfo = &II3;

            VkDescriptorImageInfo II4{};
            II4.sampler = transmittanceLUT->GetSampler();
            II4.imageView = transmittanceLUT->GetView();
            II4.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w4{};
            w4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w4.dstSet = descSet_->GetDescriptorSet();
            w4.dstBinding = 4;
            w4.descriptorCount = 1;
            w4.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w4.pImageInfo = &II4;

            VkDescriptorImageInfo II5{};
            II5.sampler = msLUT->GetSampler();
            II5.imageView = msLUT->GetView();
            II5.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w5{};
            w5.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w5.dstSet = descSet_->GetDescriptorSet();
            w5.dstBinding = 5;
            w5.descriptorCount = 1;
            w5.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w5.pImageInfo = &II5;

            DescriptorSet::Update({w0, w1, w2, w3, w4, w5});
        }

        Image3d *GetAerialPerspectiveColorRGBTransR() const { return aerialPerspColorRGBTransR_.get(); }
        Image3d *GetAerialPerspectiveTransGB() const { return aerialPerspTransGB_.get(); }
        Image2d *GetAerialPerspectiveRange() const { return aerialPerspRange_.get(); }

        void Bake(const CommandBuffer &cmd, const AtmosphereFrameUBO &frameData)
        {
            ubo_->Update(frameData); // FIXED: was ubo_->Update(&AtmosphereFrameUBO{})

            if (everBaked_)
            {
                Image::InsertImageMemoryBarrier(
                    cmd, aerialPerspColorRGBTransR_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

                Image::InsertImageMemoryBarrier(
                    cmd, aerialPerspTransGB_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

                Image::InsertImageMemoryBarrier(
                    cmd, aerialPerspRange_->GetImage(),
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            }

            pipeline_->BindPipeline(cmd);
            descSet_->BindDescriptor(cmd);

            uint32_t groupsX = (width_ + 7) / 8;
            uint32_t groupsY = (height_ + 7) / 8;
            vkCmdDispatch(cmd, groupsX, groupsY, 1);

            Image::InsertImageMemoryBarrier(
                cmd, aerialPerspColorRGBTransR_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd, aerialPerspTransGB_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            Image::InsertImageMemoryBarrier(
                cmd, aerialPerspRange_->GetImage(),
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            everBaked_ = true;
            aerialPerspColorRGBTransR_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            aerialPerspTransGB_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            aerialPerspRange_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

    private:
        uint32_t width_, height_, depth_;

        std::unique_ptr<UniformBuffer> ubo_;
        std::unique_ptr<Image3d> aerialPerspColorRGBTransR_;
        std::unique_ptr<Image3d> aerialPerspTransGB_;
        std::unique_ptr<Image2d> aerialPerspRange_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        bool everBaked_ = false;
    };
}