#include "FullscreenPass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Images/Image2d.hpp>

namespace SF::Engine
{
    FullscreenPass::FullscreenPass(Pipeline::Stage stage,
                                   std::string sourceAttachment,
                                   const std::filesystem::path &shaderPath)
        : PipelinePass(stage), sourceAttachment_(std::move(sourceAttachment))
    {
        // No vertex inputs : the vertex shader generates the fullscreen triangle
        // purely from gl_VertexIndex.
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            shaderPath,
            std::vector<Shader::VertexInput>{}, // no VBO
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::None, // fullscreen pass never needs depth
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE, // single wound triangle, no culling
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            false);

        descriptorSet_ = std::make_unique<DescriptorSet>(*pipeline_);
    }

    void FullscreenPass::Render(const CommandBuffer &commandBuffer)
    {
        // Resolve the source attachment from the global attachment map.
        auto *renderSystem = RenderSystem::Get();
        auto *srcDescriptor = renderSystem->GetAttachment(sourceAttachment_);
        auto *srcImage = dynamic_cast<const Image2d *>(srcDescriptor);

        if (!srcImage)
        {
            // Attachment not yet available (e.g. first frame) : skip silently.
            return;
        }

        // Only rewrite the descriptor set when the backing image changes
        // (avoids redundant Vulkan descriptor writes every frame).
        if (srcImage != lastBoundImage_)
        {
            lastBoundImage_ = srcImage;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = srcImage->GetSampler();
            imageInfo.imageView = srcImage->GetView();
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet_->GetDescriptorSet();
            write.dstBinding = 1; // matches "binding = 1" in the shader
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;

            DescriptorSet::Update({write});
        }

        pipeline_->BindPipeline(commandBuffer);
        descriptorSet_->BindDescriptor(commandBuffer);

        // Draw the fullscreen triangle : 3 vertices, no index/vertex buffers.
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
}
