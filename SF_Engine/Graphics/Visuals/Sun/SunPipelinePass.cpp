#include "SunPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace SF::Engine
{
    static VkWriteDescriptorSet MakeUboWrite(VkDescriptorSet d, uint32_t b,
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

    SunPipelinePass::SunPipelinePass(Pipeline::Stage stage, SunParams params)
        : PipelinePass(stage), params_(params)
    {
        ubo_ = std::make_unique<UniformBuffer>(sizeof(SunUBO));
        transmittanceLUT_ = std::make_unique<TransmittanceLUT>();
        {
            CommandBuffer cmd(true); // begin
            transmittanceLUT_->Bake(cmd);
            cmd.SubmitIdle(); // blocks until compute is done
        }

        // Depth::Read : depth test ON so the disc is hidden by geometry,
        // depth write OFF so it doesn't overwrite the scene depth buffer.
        // No vertex input : fullscreen triangle generated in the vertex shader.
        // Cull = None, topology = TriangleList.
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Sun.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::Read,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);
        WriteDescriptors();
    }

    void SunPipelinePass::SetFrameData(const glm::mat4 &invProj,
                                       const glm::mat4 &invView,
                                       const glm::vec3 &sunDir,
                                       glm::vec2 screenSize)
    {
        uboData_.invProj = invProj;
        uboData_.invView = invView;
        uboData_.sunDir = glm::vec4(glm::normalize(sunDir), params_.intensity);
        uboData_.sunColor = glm::vec4(params_.color, 0.0f);
        uboData_.screenSize = screenSize;

        // Convert angular radii to cosine thresholds
        float discRad = glm::radians(params_.discAngleDeg);
        float haloRad = glm::radians(params_.haloAngleDeg);
        uboData_.discHalfAngleCos = std::cos(discRad);
        uboData_.haloHalfAngleCos = std::cos(haloRad);
        uboData_.haloStrength = params_.haloStrength;
        uboData_.bloomStrength = params_.bloomStrength;
        uboData_._pad = glm::vec2(0.0f);
    }

    void SunPipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        ubo_->Update(uboData_);

        // Re-bind UBO each frame (contents change every frame)
        VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        DescriptorSet::Update({MakeUboWrite(descSet_->GetDescriptorSet(), 0, &bi)});

        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }

    void SunPipelinePass::WriteDescriptors()
    {
        VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        // bind=1 : transmittance LUT sampler
        // After Bake() the image is in SHADER_READ_ONLY_OPTIMAL.
        VkDescriptorImageInfo ii{};
        ii.sampler = transmittanceLUT_->GetTexture()->GetSampler();
        ii.imageView = transmittanceLUT_->GetTexture()->GetView();
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w1{};
        w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet = descSet_->GetDescriptorSet();
        w1.dstBinding = 1;
        w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w1.pImageInfo = &ii;

        DescriptorSet::Update({MakeUboWrite(descSet_->GetDescriptorSet(), 0, &bi), w1});
    }

    VkWriteDescriptorSet SunPipelinePass::WUbo(VkDescriptorSet d, uint32_t b,
                                               const VkDescriptorBufferInfo *i)
    {
        return MakeUboWrite(d, b, i);
    }
}
