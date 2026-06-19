#include <ImGui/ocornut/imgui.h>

#include "CloudPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <glm/gtc/constants.hpp>

namespace SF::Engine
{
    bool CloudPipelinePass::isWindowOpen = true;

    CloudPipelinePass::CloudPipelinePass(Pipeline::Stage stage,
                                         const AtmosphereParams &params)
        : PipelinePass(stage), params_(params)
    {
        blueNoiseLUT_ = std::make_unique<BlueNoiseLUT>(128);
        {
            CommandBuffer cmd(true);
            blueNoiseLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        pWorleyLUT_ = std::make_unique<PerlinWorleyNoiseLUT>(128);
        {
            CommandBuffer cmd(true);
            pWorleyLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        transmittanceLUT_ = std::make_unique<TransmittanceLUT>(256, 64);
        {
            CommandBuffer cmd(true);
            transmittanceLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        multiScatterLUT_ = std::make_unique<MultiScatterLUT>(
            transmittanceLUT_->GetTexture(), 32, 32);
        {
            CommandBuffer cmd(true);
            multiScatterLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        atmoUBO_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));
        cloudUBO_ = std::make_unique<UniformBuffer>(sizeof(CloudUBO));

        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Clouds/Clouds.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::Read,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);
        BindDescriptors();

        isWindowOpen = true;
    }

    void CloudPipelinePass::BindDescriptors()
    {
        // binding=0 : AtmosphereFrameUBO
        VkDescriptorBufferInfo atmoBuf{atmoUBO_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w0{};
        w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet = descSet_->GetDescriptorSet();
        w0.dstBinding = 0;
        w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo = &atmoBuf;

        // binding=1 : CloudUBO
        VkDescriptorBufferInfo cloudBuf{cloudUBO_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w1{};
        w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet = descSet_->GetDescriptorSet();
        w1.dstBinding = 1;
        w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w1.pBufferInfo = &cloudBuf;

        // binding=2 : blue noise 2D
        VkDescriptorImageInfo b2{};
        b2.sampler = blueNoiseLUT_->GetTexture()->GetSampler();
        b2.imageView = blueNoiseLUT_->GetTexture()->GetView();
        b2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w2{};
        w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet = descSet_->GetDescriptorSet();
        w2.dstBinding = 2;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w2.pImageInfo = &b2;

        // binding=3 : alligator noise 3D (4-channel Nubis Cubed noise)
        VkDescriptorImageInfo b3{};
        b3.sampler = pWorleyLUT_->GetTexture()->GetSampler();
        b3.imageView = pWorleyLUT_->GetTexture()->GetView();
        b3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w3{};
        w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet = descSet_->GetDescriptorSet();
        w3.dstBinding = 3;
        w3.descriptorCount = 1;
        w3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w3.pImageInfo = &b3;

        // binding=4 : transmittance LUT
        VkDescriptorImageInfo b4{};
        b4.sampler = transmittanceLUT_->GetTexture()->GetSampler();
        b4.imageView = transmittanceLUT_->GetTexture()->GetView();
        b4.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w4{};
        w4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w4.dstSet = descSet_->GetDescriptorSet();
        w4.dstBinding = 4;
        w4.descriptorCount = 1;
        w4.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w4.pImageInfo = &b4;

        // binding=5 : multi-scatter LUT
        VkDescriptorImageInfo b5{};
        b5.sampler = multiScatterLUT_->GetTexture()->GetSampler();
        b5.imageView = multiScatterLUT_->GetTexture()->GetView();
        b5.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w5{};
        w5.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w5.dstSet = descSet_->GetDescriptorSet();
        w5.dstBinding = 5;
        w5.descriptorCount = 1;
        w5.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w5.pImageInfo = &b5;

        DescriptorSet::Update({w0, w1, w2, w3, w4, w5});
    }

    void CloudPipelinePass::SetFrameData(const glm::mat4 &invProj,
                                         const glm::mat4 &invView,
                                         const glm::vec3 &cameraPos,
                                         const glm::vec3 &planetPos,
                                         const glm::vec3 &sunDir,
                                         glm::vec2 screenSize)
    {
        const glm::vec3 sd = glm::length(sunDir) > 1e-6f
                                 ? glm::normalize(sunDir)
                                 : glm::vec3(0.577f, 0.577f, 0.577f);
        cachedSunDir_ = sd;

        const float ruToM = params_.bottomRadius / params_.renderUnitRadius;
        const glm::vec3 pos = (cameraPos - planetPos) * ruToM;

        frameData_.invProj = invProj;
        frameData_.invView = invView;
        frameData_.cameraPos = glm::vec4(pos, 0.0f);
        frameData_.planetPos = glm::vec4(0.0f);
        frameData_.sunDir = glm::vec4(sd, params_.sunIntensity);
        frameData_.bottomRadius = params_.bottomRadius;
        frameData_.topRadius = params_.topRadius;
        frameData_.renderUnitRadius = params_.renderUnitRadius;
        frameData_._p0 = 0.0f;
        frameData_.screenSize = screenSize;
        frameData_._p1 = glm::vec2(0.0f);
    }

    void CloudPipelinePass::PreRender(const CommandBuffer &cmd)
    {
    }

    void CloudPipelinePass::Render(const CommandBuffer &cmd)
    {
        if (!enabled)
            return;

        totalTime_ += 0.016f;

        atmoUBO_->Update(frameData_);
        UpdateCloudUBO();

        pipeline_->BindPipeline(cmd);
        descSet_->BindDescriptor(cmd);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void CloudPipelinePass::UpdateCloudUBO()
    {
        CloudUBO ubo{};

        ubo.cloudBottomRadius = params_.bottomRadius + minAlt;
        ubo.cloudTopRadius = params_.bottomRadius + maxAlt;
        ubo.stepCount = static_cast<float>(marchSteps);
        ubo.lightStepCount = static_cast<float>(lightMarchSteps);

        ubo.cloudDensityScale = densityScale;
        ubo.cloudCoverage = coverage;
        ubo.windSpeed = windSpeed;
        ubo.cloudType = glm::clamp(cloudType, 0.0f, 1.0f);

        ubo.time = totalTime_;
        ubo._pad0 = 0.0f;

        ubo._pad1 = 0.0f;

        cloudUBO_->Update(ubo);
    }

    void CloudPipelinePass::DrawImGuiPanel()
    {
        if (!isWindowOpen)
            return;

        ImGui::Begin("Voxel Clouds (Nubis Cubed)", &isWindowOpen);
        ImGui::Checkbox("Enabled", &enabled);

        ImGui::End();
    }

} // namespace SF::Engine
