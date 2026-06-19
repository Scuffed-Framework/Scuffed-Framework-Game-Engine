#include "SkyViewLUT.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>

namespace SF::Engine
{
    SkyViewLUT::SkyViewLUT(Image2d *transmittanceLUT, Image2d *multiScatterLUT,
                           uint32_t w, uint32_t h)
    {
        texture_ = std::make_unique<Image2d>(
            Vector2Uint{w, h},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        ubo_ = std::make_unique<UniformBuffer>(sizeof(SkyViewPushConstants));

        pipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Atmosphere/SkyViewLUT.shader");

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        auto info0 = transmittanceLUT->GetWriteDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::nullopt);
        auto info1 = multiScatterLUT->GetWriteDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::nullopt);
        auto info2 = texture_->GetWriteDescriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);

        VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w3{};
        w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet = descSet_->GetDescriptorSet();
        w3.dstBinding = 3;
        w3.descriptorCount = 1;
        w3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w3.pBufferInfo = &bi;

        VkWriteDescriptorSet w0 = info0.GetWriteDescriptorSet();
        VkWriteDescriptorSet w1 = info1.GetWriteDescriptorSet();
        VkWriteDescriptorSet w2 = info2.GetWriteDescriptorSet();
        w0.dstSet = w1.dstSet = w2.dstSet = descSet_->GetDescriptorSet();

        DescriptorSet::Update({w0, w1, w2, w3});
    }

    void SkyViewLUT::Bake(const CommandBuffer &cmd)
    {
        // upload params via UBO instead of push constants
        ubo_->Update(push_);

        if (baked_)
        {
            Image::InsertImageMemoryBarrier(
                cmd, texture_->GetImage(),
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
        }

        pipeline_->BindPipeline(cmd);
        descSet_->BindDescriptor(cmd);
        // no vkCmdPushConstants

        auto ext = texture_->GetExtent();
        vkCmdDispatch(cmd, (ext.width + 7) / 8, (ext.height + 7) / 8, 1);

        Image::InsertImageMemoryBarrier(
            cmd, texture_->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

        baked_ = true;
        texture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}