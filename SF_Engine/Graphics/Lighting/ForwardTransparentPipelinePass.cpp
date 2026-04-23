#include "ForwardTransparentPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Mesh/Vertex.hpp>

namespace SF::Engine
{
    static VkWriteDescriptorSet FwdWriteUBO(VkDescriptorSet d, uint32_t b,
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
    static VkWriteDescriptorSet FwdWriteSSBO(VkDescriptorSet d, uint32_t b,
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
    static VkWriteDescriptorSet FwdWriteImg(VkDescriptorSet d, uint32_t b,
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

    ForwardTransparentPipelinePass::ForwardTransparentPipelinePass(Pipeline::Stage stage,
                                                                   LightManager &lightManager)
        : PipelinePass(stage), lm_(lightManager)
    {
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Lighting/ForwardTransparent.shader",
            std::vector<Shader::VertexInput>{Vertex::GetVertexInput()},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::Read, // depth test, no depth write
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE, // double-sided for glass
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // Write stable buffer bindings immediately
        VkDescriptorBufferInfo frameInfo{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo lightInfo{lm_.GetLightSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo listInfo{lm_.GetLightListSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo idxInfo{lm_.GetLightIndexSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};

        DescriptorSet::Update({
            FwdWriteUBO(descSet_->GetDescriptorSet(), 0, &frameInfo),
            FwdWriteSSBO(descSet_->GetDescriptorSet(), 1, &lightInfo),
            FwdWriteSSBO(descSet_->GetDescriptorSet(), 2, &listInfo),
            FwdWriteSSBO(descSet_->GetDescriptorSet(), 3, &idxInfo),
        });
    }

    void ForwardTransparentPipelinePass::RefreshSceneDescriptors()
    {
        auto *rs = RenderSystem::Get();
        auto *hdr = dynamic_cast<const Image2d *>(rs->GetAttachment("hdr"));
        auto *depth = dynamic_cast<const ImageDepth *>(rs->GetAttachment("gbuf_depth"));
        if (!hdr || !depth)
            return;
        if (hdr == lastHDR_ && depth == lastDepth_)
            return;
        lastHDR_ = hdr;
        lastDepth_ = depth;

        auto imgInfo = [](const Image *img)
        {
            VkDescriptorImageInfo ii{};
            ii.sampler = img->GetSampler();
            ii.imageView = img->GetView();
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return ii;
        };

        VkDescriptorImageInfo hdrII = imgInfo(hdr);
        VkDescriptorImageInfo depthII = imgInfo(depth);

        DescriptorSet::Update({
            FwdWriteImg(descSet_->GetDescriptorSet(), 4, &hdrII),
            FwdWriteImg(descSet_->GetDescriptorSet(), 5, &depthII),
        });
    }

    void ForwardTransparentPipelinePass::BeginFrame(const CommandBuffer &commandBuffer)
    {
        RefreshSceneDescriptors();
        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
    }

    void ForwardTransparentPipelinePass::Render(const CommandBuffer & /*commandBuffer*/)
    {
        // Scene system calls BeginFrame() then issues per-object draws externally.
    }
}
