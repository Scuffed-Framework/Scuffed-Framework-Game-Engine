#include "Image2d.hpp"
#include "Image3d.hpp"
#include <Graphics/Descriptors/DescriptorSet.hpp>

namespace SF::Engine
{
    void bindStorageImage(DescriptorSet &ds, Image *img)
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
    void transitionToGeneral(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    void transitionToReadOnly(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    // 3-D transitions
    void transitionToGeneral3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    void transitionToReadOnly3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(
            cmd, img->GetImage(),
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }
}