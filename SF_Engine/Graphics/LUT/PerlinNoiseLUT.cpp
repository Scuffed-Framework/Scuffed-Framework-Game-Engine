#include "PerlinNoiseLUT.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>

namespace SF::Engine
{
    PerlinNoiseLUT::PerlinNoiseLUT(uint32_t size)
    {
        texture_ = std::make_unique<Image2d>(
            UVec2{size, size},
            VK_FORMAT_R8_UNORM,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        pipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Noise/PerlinNoiseLUT.shader");

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView = texture_->GetView();
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet q{};
        q.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        q.dstSet = descSet_->GetDescriptorSet();
        q.dstBinding = 0;
        q.descriptorCount = 1;
        q.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        q.pImageInfo = &imgInfo;
        DescriptorSet::Update({q});
    }

    void PerlinNoiseLUT::Bake(const CommandBuffer &cmd)
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