#include "AtmospherePipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/SharedFunctions.hpp>

namespace SF::Engine
{
    AtmospherePipelinePass::AtmospherePipelinePass(Pipeline::Stage stage,
                                                   const AtmosphereParams &params)
        : PipelinePass(stage), params_(params)
    {
        vkDeviceWaitIdle(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice());
        atmoLUTs_ = std::make_unique<AtmoLUTs>();
        ubo_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));

        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Atmosphere/Atmosphere.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::Read, // was Depth::Read
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w0{};
        w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet = descSet_->GetDescriptorSet();
        w0.dstBinding = 0;
        w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo = &bi;

        VkDescriptorImageInfo ii1{};
        ii1.sampler = atmoLUTs_->GetTransmittanceLUT()->GetTexture()->GetSampler();
        ii1.imageView = atmoLUTs_->GetTransmittanceLUT()->GetTexture()->GetView();
        ii1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w1{};
        w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet = descSet_->GetDescriptorSet();
        w1.dstBinding = 1;
        w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w1.pImageInfo = &ii1;

        VkDescriptorImageInfo ii2{};
        ii2.sampler = atmoLUTs_->GetMultiScatterLUT()->GetTexture()->GetSampler();
        ii2.imageView = atmoLUTs_->GetMultiScatterLUT()->GetTexture()->GetView();
        ii2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w2{};
        w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet = descSet_->GetDescriptorSet();
        w2.dstBinding = 2;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w2.pImageInfo = &ii2;

        VkDescriptorImageInfo ii3{};
        ii3.sampler = atmoLUTs_->GetSkyViewLUT()->GetTexture()->GetSampler();
        ii3.imageView = atmoLUTs_->GetSkyViewLUT()->GetTexture()->GetView();
        ii3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w3{};
        w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet = descSet_->GetDescriptorSet();
        w3.dstBinding = 3;
        w3.descriptorCount = 1;
        w3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w3.pImageInfo = &ii3;

        VkDescriptorImageInfo ii4{};
        ii4.sampler = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveColorRGBTransR()->GetSampler();
        ii4.imageView = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveColorRGBTransR()->GetView();
        ii4.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w4{};
        w4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w4.dstSet = descSet_->GetDescriptorSet();
        w4.dstBinding = 4;
        w4.descriptorCount = 1;
        w4.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w4.pImageInfo = &ii4;

        VkDescriptorImageInfo ii5{};
        ii5.sampler = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveTransGB()->GetSampler();
        ii5.imageView = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveTransGB()->GetView();
        ii5.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w5{};
        w5.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w5.dstSet = descSet_->GetDescriptorSet();
        w5.dstBinding = 5;
        w5.descriptorCount = 1;
        w5.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w5.pImageInfo = &ii5;

        VkDescriptorImageInfo ii6{};
        ii6.sampler = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveRange()->GetSampler();
        ii6.imageView = atmoLUTs_->GetAerialPerspectiveLUT()->GetAerialPerspectiveRange()->GetView();
        ii6.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w6{};
        w6.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w6.dstSet = descSet_->GetDescriptorSet();
        w6.dstBinding = 6;
        w6.descriptorCount = 1;
        w6.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w6.pImageInfo = &ii6;

        DescriptorSet::Update({w0, w1, w2, w3, w4, w5, w6});
    }

    void AtmospherePipelinePass::SetSceneBuffers()
    {
        const Image2d *sceneColor = GetSceneHDR();
        const ImageDepth *sceneDepth = GetSceneDepth();

        if (!sceneColor || !sceneDepth)
            return;

        // Avoid redundant descriptor writes if nothing changed.
        if (sceneColor == lastColor_ && sceneDepth == lastDepth_)
            return;
        lastColor_ = sceneColor;
        lastDepth_ = sceneDepth;

        assert(sceneColor != nullptr);
        printf("1\n");
        VkDescriptorImageInfo ci{};
        ci.sampler = sceneColor->GetSampler();
        printf("1\n");
        ci.imageView = sceneColor->GetView();
        printf("1\n");
        ci.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet wc{};
        wc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wc.dstSet = descSet_->GetDescriptorSet();
        wc.dstBinding = 7;
        wc.descriptorCount = 1;
        wc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wc.pImageInfo = &ci;

        printf("2\n");
        VkDescriptorImageInfo di{};
        di.sampler = sceneDepth->GetSampler();
        di.imageView = sceneDepth->GetView();
        di.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet wd{};
        wd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wd.dstSet = descSet_->GetDescriptorSet();
        wd.dstBinding = 8;
        wd.descriptorCount = 1;
        wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wd.pImageInfo = &di;

        printf("3\n");
        DescriptorSet::Update({wc, wd});
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
        glm::vec3 viewPosSI = glm::vec3(
            (glm::dvec3(cameraPos) - glm::dvec3(planetPos)) * (double)ruToM);

        frameData_.invProj = invProj;
        frameData_.invView = invView;
        frameData_.cameraPos = glm::vec4(viewPosSI, 0.0f);
        frameData_.planetPos = glm::vec4(0.0f);
        frameData_.sunDir = glm::vec4(sd, params_.sunIntensity);
        frameData_.bottomRadius = params_.bottomRadius;
        frameData_.topRadius = params_.topRadius;
        frameData_.renderUnitRadius = params_.renderUnitRadius;
        frameData_._p0 = 0.0f;
        frameData_.screenSize = screenSize;
        frameData_._p1 = glm::vec2(0.0f);

        SkyViewPushConstants svp{};
        svp.sunDir = glm::vec4(sd, params_.sunIntensity);
        svp.cameraHeight = glm::length(viewPosSI) - params_.bottomRadius;
        svp.bottomRadius = params_.bottomRadius;
        svp.topRadius = params_.topRadius;
        svp._pad = 0.0f;
        svp.cameraPos = glm::vec4(viewPosSI, 0.0f);
        atmoLUTs_->GetSkyViewLUT()->SetParams(svp);
    }

    void AtmospherePipelinePass::PreRender(const CommandBuffer &commandBuffer)
    {
        ubo_->Update(frameData_);

        // SkyView: re-baked every frame (sun angle / camera height may change)
        atmoLUTs_->GetSkyViewLUT()->Bake(commandBuffer);

        // Aerial perspective: re-baked every frame (depends on camera matrices +
        // position).  Cost is negligible: 4×4 workgroups, 32 slice iterations.
        atmoLUTs_->GetAerialPerspectiveLUT()->Bake(commandBuffer, frameData_);
        SetSceneBuffers(); // call again to update
    }

    void AtmospherePipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
}