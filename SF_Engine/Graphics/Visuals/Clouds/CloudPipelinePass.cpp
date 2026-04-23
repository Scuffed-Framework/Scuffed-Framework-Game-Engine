#include "CloudPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Engine/Log/Log.hpp>

namespace SF::Engine
{
    // =========================================================================
    // Constructor
    // =========================================================================
    CloudPipelinePass::CloudPipelinePass(Pipeline::Stage compositeStage)
        : PipelinePass(compositeStage)
    {
        device_ = *RenderSystem::Get()->GetLogicalDevice();

        // Safe default: non-zero so normalize() in the shader never produces NaN.
        uboData_.sunDir = glm::normalize(glm::vec3(0.577f, 0.577f, 0.577f));

        // Bake LUTs
        basicNoise_ = std::make_unique<WorleyNoiseLUT3D>(128);
        {
            CommandBuffer cmd(true);
            basicNoise_->Bake(cmd);
            cmd.SubmitIdle();
        }

        detailNoise_ = std::make_unique<CurlNoiseLUT3D>(32);
        {
            CommandBuffer cmd(true);
            detailNoise_->Bake(cmd);
            cmd.SubmitIdle();
        }

        coverage_ = std::make_unique<CoverageLUT>(512, 512);
        {
            CommandBuffer cmd(true);
            coverage_->Bake(cmd);
            cmd.SubmitIdle();
        }

        blueNoise_ = std::make_unique<BlueNoiseLUT>(128);
        {
            CommandBuffer cmd(true);
            blueNoise_->Bake(cmd);
            cmd.SubmitIdle();
        }

        shadowLUT_ = std::make_unique<ShadowLUT>(256, 256);
        {
            CommandBuffer cmd(true);
            shadowLUT_->Bake(cmd);
            cmd.SubmitIdle();
        }

        ubo_ = std::make_unique<UniformBuffer>(sizeof(CloudFrameUBO));

        // Offscreen buffer at half swapchain resolution
        auto *rs = RenderSystem::Get();
        auto ext = rs->GetRenderStage(0)->GetRenderArea().GetExtent();
        createOffscreenBuffer(std::max(1u, ext.x / 2),
                              std::max(1u, ext.y / 2));

        // Initialise screenSize to match the actual offscreen buffer so the
        // shader can reconstruct correct NDC on the very first frame.
        uboData_.screenSize = glm::vec2(float(offW_), float(offH_));

        createRaymarchPipeline();
        createCompositePipeline();
        writeRaymarchDescriptors();
        writeCompositeDescriptors();
    }

    // =========================================================================
    // Destructor
    // =========================================================================
    CloudPipelinePass::~CloudPipelinePass()
    {
        // Wait for GPU before destroying Vulkan objects owned here.
        if (device_ != VK_NULL_HANDLE)
            vkDeviceWaitIdle(device_);
        destroyOffscreenVkObjects();
    }

    // =========================================================================
    // SetFrameData
    // =========================================================================
    void CloudPipelinePass::SetFrameData(const glm::mat4 &invViewProj,
                                         const glm::vec3 &cameraPos,
                                         const glm::vec3 &sunDir,
                                         float deltaTime)
    {
        uboData_.invViewProj = invViewProj;
        uboData_.cameraPos = cameraPos;
        uboData_.sunDir = (glm::length(sunDir) > 1e-4f)
                              ? glm::normalize(sunDir)
                              : glm::vec3(0.577f, 0.577f, 0.577f);
        uboData_.time += deltaTime;
        uboData_.windOffset += windVelocity * deltaTime;
    }

    // =========================================================================
    // OnResize
    // =========================================================================
    void CloudPipelinePass::OnResize(uint32_t fullWidth, uint32_t fullHeight)
    {
        uint32_t newW = std::max(1u, fullWidth / 2);
        uint32_t newH = std::max(1u, fullHeight / 2);
        if (newW == offW_ && newH == offH_)
            return;

        vkDeviceWaitIdle(device_);
        destroyOffscreenVkObjects();
        createOffscreenBuffer(newW, newH);
        uboData_.screenSize = glm::vec2(float(newW), float(newH));

        // The raymarch pipeline was compiled against the OLD offscreenRP_ handle.
        // offscreenRP_ is a new handle now, so the pipeline must be recreated.
        raymarchPipeline_.reset();
        raymarchDescSet_.reset();
        createRaymarchPipeline();
        writeRaymarchDescriptors();

        writeCompositeDescriptors(); // re-bind the new cloud buffer image view
        Log::Info("CloudPipelinePass: resized to {}x{}", newW, newH);
    }

    // =========================================================================
    // PreRender  called OUTSIDE the main renderpass
    // Perform the offscreen raymarch here: barrier in, nested RP, barrier out.
    // =========================================================================
    void CloudPipelinePass::PreRender(const CommandBuffer &cmd)
    {
        // Upload UBO once per frame here so Render() needs no extra update.
        ubo_->Update(uboData_);

        // Re-bind UBO descriptor (contents changed)
        {
            VkDescriptorBufferInfo bi{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = raymarchDescSet_->GetDescriptorSet();
            w.dstBinding = 5;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &bi;
            DescriptorSet::Update({w});
        }

        // Transition cloud buffer: SHADER_READ_ONLY → COLOR_ATTACHMENT
        Image::InsertImageMemoryBarrier(
            cmd, cloudBuffer_->GetImage(),
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

        // Begin offscreen renderpass
        VkClearValue clearVal{};
        clearVal.color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // zero light, full transmittance

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = offscreenRP_;
        rpbi.framebuffer = offscreenFB_;
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = {offW_, offH_};
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clearVal;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{0.0f, 0.0f, float(offW_), float(offH_), 0.0f, 1.0f};
        VkRect2D sc{{0, 0}, {offW_, offH_}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        raymarchPipeline_->BindPipeline(cmd);
        raymarchDescSet_->BindDescriptor(cmd);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        // Transition cloud buffer back: COLOR_ATTACHMENT → SHADER_READ_ONLY
        Image::InsertImageMemoryBarrier(
            cmd, cloudBuffer_->GetImage(),
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

        cloudBuffer_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // =========================================================================
    // Render  called INSIDE the main renderpass
    // Composite the half-res cloud buffer over the swapchain.
    // =========================================================================
    void CloudPipelinePass::Render(const CommandBuffer &cmd)
    {
        compositePipeline_->BindPipeline(cmd);
        compositeDescSet_->BindDescriptor(cmd);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    // =========================================================================
    // Private helpers
    // =========================================================================

    void CloudPipelinePass::destroyOffscreenVkObjects()
    {
        if (offscreenFB_ != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device_, offscreenFB_, nullptr);
            offscreenFB_ = VK_NULL_HANDLE;
        }
        if (offscreenRP_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, offscreenRP_, nullptr);
            offscreenRP_ = VK_NULL_HANDLE;
        }
    }

    void CloudPipelinePass::createOffscreenBuffer(uint32_t w, uint32_t h)
    {
        offW_ = w;
        offH_ = h;

        cloudBuffer_ = std::make_unique<Image2d>(
            Vector2Uint{w, h},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLE_COUNT_1_BIT,
            false,
            false);

        // Offscreen renderpass
        VkAttachmentDescription colAtt{};
        colAtt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        colAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        colAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colAtt.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colRef;

        // No subpass dependencies needed  the barriers we record explicitly
        // around vkCmdBeginRenderPass handle all synchronisation.
        VkRenderPassCreateInfo rpci{};
        rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = 1;
        rpci.pAttachments = &colAtt;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 0;
        rpci.pDependencies = nullptr;
        RenderSystem::CheckVkResult(vkCreateRenderPass(device_, &rpci, nullptr, &offscreenRP_));

        VkImageView view = cloudBuffer_->GetView();
        VkFramebufferCreateInfo fbci{};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = offscreenRP_;
        fbci.attachmentCount = 1;
        fbci.pAttachments = &view;
        fbci.width = w;
        fbci.height = h;
        fbci.layers = 1;
        RenderSystem::CheckVkResult(vkCreateFramebuffer(device_, &fbci, nullptr, &offscreenFB_));

        // Transition the image from UNDEFINED (its true GPU-side layout after
        // creation) into SHADER_READ_ONLY_OPTIMAL so that the first frame's
        // PreRender() barrier finds a consistent oldLayout.
        {
            CommandBuffer cmd(true);
            Image::InsertImageMemoryBarrier(
                cmd, cloudBuffer_->GetImage(),
                0, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            cmd.SubmitIdle();
        }

        Log::Info("CloudPipelinePass: offscreen buffer {}x{} created", w, h);
    }

    void CloudPipelinePass::createRaymarchPipeline()
    {
        raymarchPipeline_ = std::make_unique<RenderPipeline>(
            offscreenRP_, 0,
            "Shaders/Clouds/CloudRaymarch.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Depth::None,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        raymarchDescSet_ = std::make_unique<DescriptorSet>(*raymarchPipeline_);
    }

    void CloudPipelinePass::createCompositePipeline()
    {
        // compositeStage_ = the Pipeline::Stage passed to our constructor
        // (same stage as atmosphere/sun: {0, 0}).
        // Depth::None  composite over everything, no depth involvement.
        compositePipeline_ = std::make_unique<RenderPipeline>(
            GetStage(),
            "Shaders/Clouds/CloudComposite.shader",
            std::vector<Shader::VertexInput>{},
            std::vector<Shader::Define>{},
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::None,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        compositeDescSet_ = std::make_unique<DescriptorSet>(*compositePipeline_);
    }

    void CloudPipelinePass::writeRaymarchDescriptors()
    {
        VkDescriptorSet ds = raymarchDescSet_->GetDescriptorSet();

        static VkDescriptorImageInfo lutInfos[5];
        auto makeLutWrite = [&](uint32_t b, Image *img) -> VkWriteDescriptorSet
        {
            lutInfos[b] = {img->GetSampler(), img->GetView(),
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = b;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &lutInfos[b];
            return w;
        };

        static VkDescriptorBufferInfo uboInfo;
        uboInfo = {ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet wUbo{};
        wUbo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wUbo.dstSet = ds;
        wUbo.dstBinding = 5;
        wUbo.descriptorCount = 1;
        wUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wUbo.pBufferInfo = &uboInfo;

        DescriptorSet::Update({
            makeLutWrite(0, basicNoise_->GetTexture()),
            makeLutWrite(1, detailNoise_->GetTexture()),
            makeLutWrite(2, coverage_->GetTexture()),
            makeLutWrite(3, blueNoise_->GetTexture()),
            makeLutWrite(4, shadowLUT_->GetTexture()),
            wUbo,
        });
    }

    void CloudPipelinePass::writeCompositeDescriptors()
    {
        static VkDescriptorImageInfo cbInfo;
        cbInfo = {cloudBuffer_->GetSampler(), cloudBuffer_->GetView(),
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = compositeDescSet_->GetDescriptorSet();
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &cbInfo;

        DescriptorSet::Update({w});
    }

} // namespace SF::Engine
