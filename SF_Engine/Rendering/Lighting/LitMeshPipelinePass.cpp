#include "LitMeshPipelinePass.hpp"

#include <Rendering/RenderSystem.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Bitmaps/Bitmap.hpp>

namespace SF::Engine
{
    static VkWriteDescriptorSet WUbo(VkDescriptorSet d, uint32_t b, const VkDescriptorBufferInfo *i)
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
    static VkWriteDescriptorSet WSsbo(VkDescriptorSet d, uint32_t b, const VkDescriptorBufferInfo *i)
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
    static VkWriteDescriptorSet WImg(VkDescriptorSet d, uint32_t b, const VkDescriptorImageInfo *i)
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
    static VkDescriptorImageInfo ImgInfo(const Image2d *img)
    {
        VkDescriptorImageInfo ii{};
        ii.sampler = img->GetSampler();
        ii.imageView = img->GetView();
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return ii;
    }

    // Creates a 1x1 Image2d with the given RGBA bytes, fully uploaded to GPU.
    static std::unique_ptr<Image2d> Make1x1(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        auto bmp = std::make_unique<Bitmap>(UVec2{1, 1}, 4);
        uint8_t *p = bmp->GetData().get();
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
        return std::make_unique<Image2d>(
            std::move(bmp),
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLE_COUNT_1_BIT,
            false, false);
    }

    LitMeshPipelinePass::LitMeshPipelinePass(Pipeline::Stage stage, LightManager &lightManager)
        : PipelinePass(stage), lm_(lightManager)
    {
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Lit.shader",
            std::vector<Shader::VertexInput>{Vertex::GetVertexInput()},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::ReadWrite,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // Fallback textures with actual pixel data uploaded to GPU
        fallbackWhite_ = Make1x1(255, 255, 255, 255);  // white  : albedo / pbr / emissive
        fallbackNormal_ = Make1x1(128, 128, 255, 255); // flat normal : tangent-space up

        WriteFrameDescriptors();

        // Prime image bindings with fallbacks : must be valid before first draw
        MeshMaterial empty{};
        WriteMaterialDescriptors(empty);
    }

    void LitMeshPipelinePass::Submit(std::shared_ptr<Mesh> mesh,
                                     const MeshMaterial &material,
                                     const Mat4 &transform)
    {
        drawList_.push_back({std::move(mesh), material, transform});
    }

    void LitMeshPipelinePass::Submit(const MeshInstance &instance)
    {
        drawList_.push_back({instance.mesh, instance.material, instance.transform});
    }

    void LitMeshPipelinePass::WriteFrameDescriptors()
    {
        VkDescriptorBufferInfo frameInfo{lm_.GetFrameUBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo lightInfo{lm_.GetLightSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo listInfo{lm_.GetLightListSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo idxInfo{lm_.GetLightIndexSSBO().GetBuffer(), 0, VK_WHOLE_SIZE};

        DescriptorSet::Update({
            WUbo(descSet_->GetDescriptorSet(), 0, &frameInfo),
            WSsbo(descSet_->GetDescriptorSet(), 1, &lightInfo),
            WSsbo(descSet_->GetDescriptorSet(), 2, &listInfo),
            WSsbo(descSet_->GetDescriptorSet(), 3, &idxInfo),
        });
    }

    void LitMeshPipelinePass::WriteMaterialDescriptors(const MeshMaterial &mat)
    {
        Image2d *albedo = mat.albedo ? mat.albedo.get() : fallbackWhite_.get();
        Image2d *normal = mat.normal ? mat.normal.get() : fallbackNormal_.get();
        Image2d *pbr = mat.pbr ? mat.pbr.get() : fallbackWhite_.get();
        Image2d *emissive = mat.emissive ? mat.emissive.get() : fallbackWhite_.get();

        VkDescriptorImageInfo ai = ImgInfo(albedo);
        VkDescriptorImageInfo ni = ImgInfo(normal);
        VkDescriptorImageInfo pi = ImgInfo(pbr);
        VkDescriptorImageInfo ei = ImgInfo(emissive);

        DescriptorSet::Update({
            WImg(descSet_->GetDescriptorSet(), 4, &ai),
            WImg(descSet_->GetDescriptorSet(), 5, &ni),
            WImg(descSet_->GetDescriptorSet(), 6, &pi),
            WImg(descSet_->GetDescriptorSet(), 7, &ei),
        });
    }

    void LitMeshPipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        if (drawList_.empty())
            return;

        pipeline_->BindPipeline(commandBuffer);

        for (auto &dc : drawList_)
        {
            WriteMaterialDescriptors(dc.material);
            descSet_->BindDescriptor(commandBuffer); // moved inside loop, after the write

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
