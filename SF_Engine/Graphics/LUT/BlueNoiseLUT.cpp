#include "BlueNoiseLUT.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Engine/Log/Log.hpp>
#include <UtilityClasses/VKMemDump.hpp>

namespace SF::Engine
{

    // Internal helpers

    // Bind a storage image at binding 0 of a descriptor set.
    static void bindStorageImage(DescriptorSet &ds, Image *img)
    {
        VkDescriptorImageInfo imgInfo{};
        imgInfo.sampler = img->GetSampler();
        imgInfo.imageView = img->GetView();
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = ds.GetDescriptorSet();
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo = &imgInfo;
        DescriptorSet::Update({w});
    }

    // 2-D transitions
    static void transitionToGeneral(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    static void transitionToReadOnly(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    // 3-D transitions
    static void transitionToGeneral3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    static void transitionToReadOnly3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    // BlueNoiseLUT  (2-D R16_SFLOAT)

    BlueNoiseLUT::BlueNoiseLUT(uint32_t size)
        : size_(size)
    {
        texture_ = std::make_unique<Image2d>(
            Vector2Uint{size_, size_},
            VK_FORMAT_R16_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLE_COUNT_1_BIT,
            false, false);
        createPipeline();
    }

    void BlueNoiseLUT::createPipeline()
    {
        pipeline_ = std::make_unique<ComputePipeline>("Shaders/Noise/BlueNoiseLUT.shader");
        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);
        bindStorageImage(*descSet_, texture_.get());
    }

    void BlueNoiseLUT::Bake(const CommandBuffer &cmd)
    {
        if (baked_)
            transitionToGeneral(cmd, texture_.get());

        pipeline_->BindPipeline(cmd);
        descSet_->BindDescriptor(cmd);
        vkCmdDispatch(cmd, (size_ + 7) / 8, (size_ + 7) / 8, 1);

        transitionToReadOnly(cmd, texture_.get());
        texture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        baked_ = true;
        Log::Info("BlueNoiseLUT::Bake complete ({}x{})", size_, size_);
    }

    PerlinWorleyNoiseLUT::PerlinWorleyNoiseLUT(uint32_t size)
        : size_(size)
    {
        texture_ = std::make_unique<Image3d>(
            VkExtent3D{size_, size_, size_},
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLE_COUNT_1_BIT,
            false, false);
        createPipeline();
    }

    void PerlinWorleyNoiseLUT::createPipeline()
    {
        pipeline_ = std::make_unique<ComputePipeline>("Shaders/Noise/PerlinWorleyNoiseLUT.shader");
        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);
        bindStorageImage(*descSet_, texture_.get());
    }

    void PerlinWorleyNoiseLUT::Bake(const CommandBuffer &cmd)
    {
        if (baked_)
            transitionToGeneral3d(cmd, texture_.get());

        pipeline_->BindPipeline(cmd);
        descSet_->BindDescriptor(cmd);
        const uint32_t g = (size_ + 3) / 4;
        vkCmdDispatch(cmd, g, g, g);

        transitionToReadOnly3d(cmd, texture_.get());
        texture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        baked_ = true;
        Log::Info("PerlinWorleyNoiseLUT::Bake complete ({}^3)", size_);
    }
} // namespace SF::Engine
