#include "MultiScatterLUT.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>

namespace SF::Engine
{
    MultiScatterLUT::MultiScatterLUT(Image2d *transmittanceLUT, uint32_t w, uint32_t h)
    {
        texture_ = std::make_unique<Image2d>(
            Vector2Uint{w, h},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        pipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Atmosphere/MultiScatterLUT.shader");

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // Keep WriteDescriptorSetInformation alive  it owns the pImageInfo pointer
        auto info0 = transmittanceLUT->GetWriteDescriptor(
            0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::nullopt);
        auto info1 = texture_->GetWriteDescriptor(
            1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::nullopt);

        // Patch dstSet  GetWriteDescriptor leaves it null
        VkWriteDescriptorSet w0 = info0.GetWriteDescriptorSet();
        VkWriteDescriptorSet w1 = info1.GetWriteDescriptorSet();
        w0.dstSet = descSet_->GetDescriptorSet();
        w1.dstSet = descSet_->GetDescriptorSet();

        DescriptorSet::Update(std::vector<VkWriteDescriptorSet>{w0, w1});
        // info0, info1 (and their pImageInfo memory) are still alive here
    }

    void MultiScatterLUT::Bake(const CommandBuffer &cmd)
    {
        // On re-bake the image may already be SHADER_READ_ONLY_OPTIMAL; transition back.
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

        auto ext = texture_->GetExtent();
        vkCmdDispatch(cmd, ext.width, ext.height, 1);

        Image::InsertImageMemoryBarrier(
            cmd,
            texture_->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1, 0, 1, 0);

        baked_ = true;
        texture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}