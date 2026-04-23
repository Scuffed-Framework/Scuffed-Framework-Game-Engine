#include "DeferredLightPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Images/ImageDepth.hpp>

namespace SF::Engine
{
    //  helper lambdas
    static VkWriteDescriptorSet WriteUBO(VkDescriptorSet d, uint32_t b,
                                         const VkDescriptorBufferInfo *i)
    {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d;
        w.dstBinding = b;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = i;
        return w;
    }
    static VkWriteDescriptorSet WriteSSBO(VkDescriptorSet d, uint32_t b,
                                          const VkDescriptorBufferInfo *i)
    {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d;
        w.dstBinding = b;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = i;
        return w;
    }
    static VkWriteDescriptorSet WriteImg(VkDescriptorSet d, uint32_t b,
                                         const VkDescriptorImageInfo *i)
    {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d;
        w.dstBinding = b;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = i;
        return w;
    }

    //  ctor
    DeferredLightPipelinePass::DeferredLightPipelinePass(Pipeline::Stage stage,
                                                         LightManager &lightManager)
        : PipelinePass(stage), lm_(lightManager)
    {
        // Fullscreen triangle : no vertex input, no depth
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Lighting/DeferredLight.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::None,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Single descriptor set : all bindings (buffers + gbuffer textures) in one set.
        // The textures are written lazily each frame in RefreshGBufferDescriptors().
        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // Write the stable buffer bindings immediately (pointers never change)
        VkDescriptorBufferInfo frameInfo{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo lightInfo{lm_.GetLightSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo listInfo{lm_.GetLightListSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo idxInfo{lm_.GetLightIndexSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};

        DescriptorSet::Update({
            WriteUBO(descSet_->GetDescriptorSet(), 0, &frameInfo),
            WriteSSBO(descSet_->GetDescriptorSet(), 1, &lightInfo),
            WriteSSBO(descSet_->GetDescriptorSet(), 2, &listInfo),
            WriteSSBO(descSet_->GetDescriptorSet(), 3, &idxInfo),
        });
    }

    //  refresh GBuffer image descriptors
    void DeferredLightPipelinePass::RefreshGBufferDescriptors()
    {
        auto *rs = RenderSystem::Get();
        auto *albedo = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_albedo"));
        auto *normal = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_normal"));
        auto *pbr = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_pbr"));
        auto *depth = dynamic_cast<const ImageDepth *>(rs->GetAttachment("gbuf_depth"));

        if (!albedo || !normal || !pbr || !depth)
            return;
        // Pointer comparison : only rewrite if attachments were recreated
        if (albedo == lastAlbedo_ && normal == lastNormal_ &&
            pbr == lastPbr_ && depth == lastDepth_)
            return;

        lastAlbedo_ = albedo;
        lastNormal_ = normal;
        lastPbr_ = pbr;
        lastDepth_ = depth;

        auto imgInfo = [](const Image *img)
        {
            VkDescriptorImageInfo ii{};
            ii.sampler = img->GetSampler();
            ii.imageView = img->GetView();
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return ii;
        };

        // Bindings 4-7 are the GBuffer textures (see DeferredLight.shader set=1 bind=0..3
        // but since the engine uses a single flat descriptor set per pipeline via SPIR-V
        // reflection, they appear at the binding numbers declared in the shader).
        VkDescriptorImageInfo albedoII = imgInfo(albedo);
        VkDescriptorImageInfo normalII = imgInfo(normal);
        VkDescriptorImageInfo pbrII = imgInfo(pbr);
        VkDescriptorImageInfo depthII = imgInfo(depth);

        DescriptorSet::Update({
            WriteImg(descSet_->GetDescriptorSet(), 4, &albedoII),
            WriteImg(descSet_->GetDescriptorSet(), 5, &normalII),
            WriteImg(descSet_->GetDescriptorSet(), 6, &pbrII),
            WriteImg(descSet_->GetDescriptorSet(), 7, &depthII),
        });
    }

    //  render
    void DeferredLightPipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        RefreshGBufferDescriptors();
        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
}
