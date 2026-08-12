#include "GBufferPass.hpp"
#include <Rendering/RenderSystem.hpp>
namespace SF::Engine
{
    //  helpers (same pattern as LitMeshPipelinePass)
    static VkWriteDescriptorSet GBufWUbo(VkDescriptorSet d, uint32_t b,
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
    static VkWriteDescriptorSet GBufWImg(VkDescriptorSet d, uint32_t b,
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
    static VkDescriptorImageInfo GBufImgInfo(const Image2d *img)
    {
        VkDescriptorImageInfo ii{};
        ii.sampler = img->GetSampler();
        ii.imageView = img->GetView();
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return ii;
    }

    //  ctor
    GBufferPass::GBufferPass(Pipeline::Stage stage, LightManager &lightManager)
        : PipelinePass(stage), lm_(lightManager)
    {
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Lighting/GBuffer.shader",
            std::vector<Shader::VertexInput>{Vertex::GetVertexInput()},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::MRT,
            RenderPipeline::Depth::ReadWrite,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // 1×1 fallback textures
        UVec2 one{1, 1};
        fallbackWhite_ = std::make_unique<Image2d>(
            one, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLE_COUNT_1_BIT, false, false);

        uint8_t whitePixels[4] = {255, 255, 255, 255};
        fallbackWhite_->SetPixels(whitePixels, 1, 0);

        fallbackNormal_ = std::make_unique<Image2d>(
            one, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLE_COUNT_1_BIT, false, false);

        uint8_t normalPixels[4] = {128, 128, 255, 255};
        fallbackNormal_->SetPixels(normalPixels, 1, 0);
        WriteFrameDescriptors();
        WriteMaterialDescriptors(MeshMaterial{});
    }

    //  Submit
    void GBufferPass::Submit(std::shared_ptr<Mesh> mesh,
                             const MeshMaterial &material,
                             const Mat4 &transform)
    {
        drawList_.push_back({std::move(mesh), material, transform});
    }

    //  WriteFrameDescriptors
    void GBufferPass::WriteFrameDescriptors()
    {
        VkDescriptorBufferInfo frameInfo{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        DescriptorSet::Update({
            GBufWUbo(descSet_->GetDescriptorSet(), 0, &frameInfo),
        });
    }

    //  WriteMaterialDescriptors
    void GBufferPass::WriteMaterialDescriptors(const MeshMaterial &mat)
    {
        Image2d *albedo = mat.albedo ? mat.albedo.get() : fallbackWhite_.get();
        Image2d *normal = mat.normal ? mat.normal.get() : fallbackNormal_.get();
        Image2d *pbr = mat.pbr ? mat.pbr.get() : fallbackWhite_.get();
        Image2d *emissive = mat.emissive ? mat.emissive.get() : fallbackWhite_.get();

        VkDescriptorImageInfo ai = GBufImgInfo(albedo);
        VkDescriptorImageInfo ni = GBufImgInfo(normal);
        VkDescriptorImageInfo pi = GBufImgInfo(pbr);
        VkDescriptorImageInfo ei = GBufImgInfo(emissive);

        DescriptorSet::Update({
            GBufWImg(descSet_->GetDescriptorSet(), 1, &ai),
            GBufWImg(descSet_->GetDescriptorSet(), 2, &ni),
            GBufWImg(descSet_->GetDescriptorSet(), 3, &pi),
            GBufWImg(descSet_->GetDescriptorSet(), 4, &ei),
        });
    }

    //  Render
    void GBufferPass::Render(const CommandBuffer &commandBuffer)
    {
        if (drawList_.empty())
            return;

        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);

        for (auto &dc : drawList_)
        {
            WriteMaterialDescriptors(dc.material);

            LitPushConstants pc{};
            pc.model = dc.transform;
            pc.baseColor = dc.material.baseColor;
            pc.roughnessFactor = dc.material.roughnessFactor;
            pc.metallicFactor = dc.material.metallicFactor;
            pc.aoFactor = dc.material.aoFactor;
            pc.emissiveFactor = dc.material.emissiveFactor;

            vkCmdPushConstants(
                commandBuffer,
                pipeline_->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(LitPushConstants), &pc);

            dc.mesh->Draw(commandBuffer);
        }

        drawList_.clear();
    }
}
