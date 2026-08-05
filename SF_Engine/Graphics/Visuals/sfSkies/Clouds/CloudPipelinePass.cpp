#include <ImGui/ocornut/imgui.h>

#include "CloudPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <glm/gtc/constants.hpp>
#include <Graphics/SharedFunctions.hpp>
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

        cloudNoise_ = std::make_unique<CloudNoiseLUTs>(128, 128, 128);
        {
            CommandBuffer cmd(true);
            cloudNoise_->Bake(cmd);
            cmd.SubmitIdle();
        }

        aerialPerspRange_ = std::make_unique<AerialPerspectiveLUT>(
            transmittanceLUT_->GetTexture(), multiScatterLUT_->GetTexture(), 128, 32);
        {
            CommandBuffer cmd(true);
            aerialPerspRange_->Bake(cmd, AtmosphereFrameUBO());
            cmd.SubmitIdle();
        }

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
        // this fails
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
        b2.imageView = blueNoiseLUT_->GetTexture()->GetView();
        b2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w2{};
        w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet = descSet_->GetDescriptorSet();
        w2.dstBinding = 2;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w2.pImageInfo = &b2;

        // binding=3 : alligator noise 3D (4-channel Nubis Cubed noise)
        VkDescriptorImageInfo b3{};
        b3.imageView = pWorleyLUT_->GetTexture()->GetView();
        b3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w3{};
        w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet = descSet_->GetDescriptorSet();
        w3.dstBinding = 3;  
        w3.descriptorCount = 1;
        w3.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w3.pImageInfo = &b3;

        // binding=4 : transmittance LUT
        VkDescriptorImageInfo b4{};
        b4.imageView = transmittanceLUT_->GetTexture()->GetView();
        b4.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w4{};
        w4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w4.dstSet = descSet_->GetDescriptorSet();
        w4.dstBinding = 4;
        w4.descriptorCount = 1;
        w4.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w4.pImageInfo = &b4;

        // binding=5 : multi-scatter LUT
        VkDescriptorImageInfo b5{};
        b5.imageView = multiScatterLUT_->GetTexture()->GetView();
        b5.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w5{};
        w5.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w5.dstSet = descSet_->GetDescriptorSet();
        w5.dstBinding = 5;
        w5.descriptorCount = 1;
        w5.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w5.pImageInfo = &b5;

        VkDescriptorImageInfo b6{};
        b6.imageView = cloudNoise_->GetBaseTexture()->GetView();
        b6.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w6{};
        w6.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w6.dstSet = descSet_->GetDescriptorSet();
        w6.dstBinding = 6;
        w6.descriptorCount = 1;
        w6.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w6.pImageInfo = &b6;

        VkDescriptorImageInfo b7{};
        b7.imageView = cloudNoise_->GetDetailTexture()->GetView();
        b7.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w7{};
        w7.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w7.dstSet = descSet_->GetDescriptorSet();
        w7.dstBinding = 7;
        w7.descriptorCount = 1;
        w7.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w7.pImageInfo = &b7;

        VkDescriptorImageInfo b8{};
        // spared because it was too funny
        // b8.sampler = aerialPerspRange_->GetAerialPerspectiveRange()->GetSampler(); // shit i forgot to initialize it
        b8.imageView = aerialPerspRange_->GetAerialPerspectiveRange()->GetView();
        b8.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w8{};
        w8.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w8.dstSet = descSet_->GetDescriptorSet();
        w8.dstBinding = 8;
        w8.descriptorCount = 1;
        w8.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w8.pImageInfo = &b8;
        
        BindSharedCameraData(11, 1, descSet_.get());

        VkDescriptorImageInfo b12{};
        b12.sampler = SharedSamplers::GetLinearRepeatSampler();
        // imageView left null — this is a pure sampler-only binding (VK_DESCRIPTOR_TYPE_SAMPLER),
        // not a combined image sampler.

        VkWriteDescriptorSet w12{};
        w12.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w12.dstSet = descSet_->GetDescriptorSet();
        w12.dstBinding = 12;
        w12.descriptorCount = 1;
        w12.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;   // not COMBINED_IMAGE_SAMPLER
        w12.pImageInfo = &b12;

        VkDescriptorImageInfo b13{};
        b13.sampler = SharedSamplers::GetLinearClampSampler();
        // imageView left null — this is a pure sampler-only binding (VK_DESCRIPTOR_TYPE_SAMPLER),
        // not a combined image sampler.

        VkWriteDescriptorSet w13{};
        w13.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w13.dstSet = descSet_->GetDescriptorSet();
        w13.dstBinding = 13;
        w13.descriptorCount = 1;
        w13.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;   // not COMBINED_IMAGE_SAMPLER
        w13.pImageInfo = &b13;

        DescriptorSet::Update({w0, w1, w2, w3, w4, w5, w6, w7, /*w8,tempdisabled*/ w12, w13});
    }

    void CloudPipelinePass::SetFrameData(const Mat4 &invProj,
                                         const Mat4 &invView,
                                         const Vec3 &cameraPos,
                                         const Vec3 &planetPos,
                                         const Vec3 &sunDir,
                                         glm::vec2 screenSize)
    {
        const Vec3 sd = glm::length(sunDir) > 1e-6f
                                 ? normalize(sunDir)
                                 : Vec3(0.577f, 0.577f, 0.577f);
        cachedSunDir_ = sd;

        const float ruToM = params_.bottomRadius / params_.renderUnitRadius;
        const Vec3 pos = (cameraPos - planetPos) * ruToM;

        frameData_.invProj = invProj;
        frameData_.invView = invView;
        frameData_.cameraPos = Vec4(pos, 0.0f);
        frameData_.planetPos = Vec4(0.0f);
        frameData_.sunDir = Vec4(sd, params_.sunIntensity);
        frameData_.bottomRadius = params_.bottomRadius;
        frameData_.topRadius = params_.topRadius;
        frameData_.renderUnitRadius = params_.renderUnitRadius;
        frameData_.screenSize = screenSize;
        frameData_.sunCol = Vec4(1);
    }

    void CloudPipelinePass::PreRender(const CommandBuffer &cmd)
    {
    }

    void CloudPipelinePass::Render(const CommandBuffer &cmd)
    {
        if (!enabled)
            return;

        auto *rs = RenderSystem::Get();
        auto *depthDesc = rs->GetAttachment("gbuf_depth");
        auto *colorDesc = rs->GetAttachment("hdr");
        auto *depthImg = dynamic_cast<const ImageDepth *>(depthDesc);
        auto *colorImg = dynamic_cast<const Image2d *>(colorDesc);
        if (!depthImg || !colorImg)
            return; // not ready yet, e.g. first frame


        VkImageView depthView = depthImg->GetView();
        VkImageView colorView = colorImg->GetView();
        VkImageView lastDepthView = lastDepthImg_ ? lastDepthImg_->GetView() : VK_NULL_HANDLE;
        VkImageView lastColorView = lastColorImg_ ? lastColorImg_->GetView() : VK_NULL_HANDLE;


        if (depthView != lastDepthView || colorView != lastColorView)
        {
            lastDepthImg_ = depthImg;
            lastColorImg_ = colorImg;

            VkDescriptorImageInfo dInfo{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet w9{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w9.dstSet = descSet_->GetDescriptorSet();
            w9.dstBinding = 9;
            w9.descriptorCount = 1;
            w9.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            w9.pImageInfo = &dInfo;

            DescriptorSet::Update({w9});
        }

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
        ubo.cloudDetailScale = cloudDetailScale;
        ubo.cloudCurlNoiseScale = cloudCurlNoiseScale;
        ubo.cloudBaseNoiseScale = cloudBaseNoiseScale;
        ubo.cloudWeatherUVScale = cloudWeatherUVScale;

        ubo.time = totalTime_;

        frameCounter_ = (frameCounter_ + 1) % 256;
        ubo.frameIndex = static_cast<int>(frameCounter_);

        cloudUBO_->Update(ubo);
    }

    void CloudPipelinePass::DrawImGuiPanel()
    {
        if (!isWindowOpen)
            return;
    }

} // namespace SF::Engine
