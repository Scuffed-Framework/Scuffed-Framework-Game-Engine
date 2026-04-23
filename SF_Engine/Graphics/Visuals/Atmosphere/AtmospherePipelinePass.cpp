#include "AtmospherePipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>

namespace SF::Engine
{
    AtmospherePipelinePass::AtmospherePipelinePass(Pipeline::Stage stage,
                                                   const AtmosphereParams &params)
        : PipelinePass(stage), params_(params)
    {
        //  Transmittance LUT  baked once on a one-shot command buffer
        transmittanceLUT_ = std::make_unique<TransmittanceLUT>(256, 64);
        {
            CommandBuffer cmd(true); // begin = true
            transmittanceLUT_->Bake(cmd);
            cmd.SubmitIdle(); // blocks until compute is done
        }
        multiScatterLUT_ = std::make_unique<MultiScatterLUT>(
            transmittanceLUT_->GetTexture(), 32, 32);
        {
            CommandBuffer cmd(true);
            multiScatterLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        //  Sky UBO
        ubo_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));

        //  Sky pipeline
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Atmosphere/Atmosphere.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::None, // renders first as sky background; geometry draws on top
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // bind=0 : frame UBO
        VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w0{};
        w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet = descSet_->GetDescriptorSet();
        w0.dstBinding = 0;
        w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo = &bi;

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

        VkDescriptorImageInfo ii2{};
        ii2.sampler = multiScatterLUT_->GetTexture()->GetSampler();
        ii2.imageView = multiScatterLUT_->GetTexture()->GetView();
        ii2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w2{};
        w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet = descSet_->GetDescriptorSet();
        w2.dstBinding = 2;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w2.pImageInfo = &ii2;

        DescriptorSet::Update({w0, w1, w2});
    }

    void AtmospherePipelinePass::SetFrameData(const glm::mat4 &invProj,
                                              const glm::mat4 &invView,
                                              const glm::vec3 &cameraPos,
                                              const glm::vec3 &planetPos,
                                              const glm::vec3 &sunDir,
                                              glm::vec2 screenSize)
    {
        const glm::vec3 sd = glm::length(sunDir) > 1e-6f
                                 ? glm::normalize(sunDir)
                                 : glm::vec3(0.577f, 0.577f, 0.577f);

        const float ruToM = params_.bottomRadius / params_.renderUnitRadius;
        const glm::vec3 viewPosSI = (cameraPos - planetPos) * ruToM;

        frameData_.invProj = invProj;
        frameData_.invView = invView;
        frameData_.cameraPos = glm::vec4(viewPosSI, 0.0f);
        frameData_.planetPos = glm::vec4(0.0f);
        frameData_.sunDir = glm::vec4(sd, params_.sunIntensity);
        frameData_.bottomRadius = params_.bottomRadius;
        frameData_.topRadius = params_.topRadius;
        frameData_.renderUnitRadius = params_.bottomRadius;
        frameData_._p0 = 0.0f;
        frameData_.screenSize = screenSize;
        frameData_._p1 = glm::vec2(0.0f);
    }

    void AtmospherePipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        ubo_->Update(frameData_);
        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
}
