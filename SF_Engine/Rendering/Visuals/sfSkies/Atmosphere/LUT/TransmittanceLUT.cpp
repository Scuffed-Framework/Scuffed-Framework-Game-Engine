#include "TransmittanceLUT.hpp"
#include <Rendering/RenderSystem.hpp>
#include <Engine/Log/Log.hpp>

namespace SF::Engine
{
    TransmittanceLUT::TransmittanceLUT(uint32_t w, uint32_t h)
    {
        texture_ = std::make_unique<Image2d>(
            UVec2{w, h},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLE_COUNT_1_BIT,
            false,  // no anisotropic
            false); // no mipmaps

        pipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Atmosphere/TransmittanceLUT.shader");

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // Bind the storage image at set=0, binding=0
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

    void TransmittanceLUT::Bake(const CommandBuffer &cmd)
    {
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
        auto size = texture_->GetSize();
        vkCmdDispatch(cmd, (size.x + 7) / 8, (size.y + 7) / 8, 1);

        Log::Info("TransmittanceLUT::Bake: image handle before final barrier = 0x{:x}",
                  (uint64_t)(VkImage)texture_->GetImage());

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
