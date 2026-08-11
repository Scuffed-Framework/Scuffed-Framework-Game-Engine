#include <Gui/ocornut/imgui.h>

#include "CloudPipelinePass.hpp"
#include <Graphics/RenderSystem.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <glm/gtc/constants.hpp>
#include <Graphics/SharedFunctions.hpp>
#include <Graphics/SharedSamplers.hpp>
#include <Graphics/Descriptors/DescriptorSetBuilder.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/LUT/AtmoLUTs.hpp>

namespace SF::Engine
{
    bool CloudPipelinePass::isWindowOpen = true;

    CloudPipelinePass::CloudPipelinePass(Pipeline::Stage stage, AtmosphereData &data)
        : PipelinePass(stage), data_(data)
    {
        PipelinePass::SetOrder(100);
        uiHandle = UIRegistry::Get().Register([this]
                                              { DrawImGuiPanel(); });

        atmoUBO_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));
        cloudUBO_ = std::make_unique<UniformBuffer>(sizeof(CloudUBO));

        cloudNoise_ = std::make_unique<CloudNoiseLUTs>(128, 128, 128);
        {
            CommandBuffer cmd(true);
            cloudNoise_->Bake(cmd);
            cmd.SubmitIdle();
        }

        UVec2 fullRes = UVec2(GetScreenSize().x, GetScreenSize().y);
        UVec2 quarterRes{fullRes.x / 4, fullRes.y / 4};

        raymarchPipeline_ = std::make_unique<ComputePipeline>("Shaders/Clouds/RaymarchClouds.shader");
        reconstructPipeline_ = std::make_unique<ComputePipeline>("Shaders/Clouds/Reconstruct.shader");
        compositePipeline_ = std::make_unique<ComputePipeline>("Shaders/Clouds/Composite.shader");

        raymarchSet_ = std::make_unique<DescriptorSet>(*raymarchPipeline_);
        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            reconstructSet_[i] = std::make_unique<DescriptorSet>(*reconstructPipeline_);
            compositeSet_[i] = std::make_unique<DescriptorSet>(*compositePipeline_);
        }

        // Create a 1x1 dummy texture for frame 0 history fallback
        dummyTexture_ = std::make_unique<Image2d>(UVec2{1, 1}, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        {
            CommandBuffer cmd(true);

            Image::InsertImageMemoryBarrier(cmd, dummyTexture_->GetImage(),
                                            0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            VkClearColorValue clearColor{};
            clearColor.float32[0] = 0.0f;
            clearColor.float32[1] = 0.0f;
            clearColor.float32[2] = 0.0f;
            clearColor.float32[3] = 1.0f;
            VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdClearColorImage(cmd, dummyTexture_->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

            Image::InsertImageMemoryBarrier(cmd, dummyTexture_->GetImage(),
                                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);

            cmd.SubmitIdle();
        }
        dummyTexture_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Create images - constructor transitions UNDEFINED -> GENERAL
        cloudRenderRT_ = std::make_unique<Image2d>(quarterRes, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        cloudDepthRT_ = std::make_unique<Image2d>(quarterRes, VK_FORMAT_R32_SFLOAT,
                                                  VK_IMAGE_LAYOUT_GENERAL,
                                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        cloudFogRT_ = std::make_unique<Image2d>(quarterRes, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                VK_IMAGE_LAYOUT_GENERAL,
                                                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            reconColor_[i] = std::make_unique<Image2d>(fullRes, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                       VK_IMAGE_LAYOUT_GENERAL,
                                                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            reconDepth_[i] = std::make_unique<Image2d>(fullRes, VK_FORMAT_R32_SFLOAT,
                                                       VK_IMAGE_LAYOUT_GENERAL,
                                                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            reconFog_[i] = std::make_unique<Image2d>(fullRes, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        }

        // Transition all images to SHADER_READ_ONLY_OPTIMAL before BindDescriptors()
        {
            CommandBuffer cmd(true);

            auto transitionToReadOnly = [&](Image2d *img)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                0, VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            };

            transitionToReadOnly(cloudRenderRT_.get());
            transitionToReadOnly(cloudDepthRT_.get());
            transitionToReadOnly(cloudFogRT_.get());

            for (uint32_t i = 0; i < kFramesInFlight; ++i)
            {
                transitionToReadOnly(reconColor_[i].get());
                transitionToReadOnly(reconDepth_[i].get());
                transitionToReadOnly(reconFog_[i].get());
            }

            cmd.SubmitIdle();
        }

        // Update CPU-side layout tracking
        cloudRenderRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        cloudDepthRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        cloudFogRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            reconColor_[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            reconDepth_[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            reconFog_[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // NOW call BindDescriptors() - images are already in SHADER_READ_ONLY_OPTIMAL
        BindDescriptors();

        isWindowOpen = true;
    }

    void CloudPipelinePass::BindDescriptors()
    {
        // --- Raymarch set ---
        auto raymarchWrites = DescriptorSetWriteBuilder(*raymarchSet_)
                                  .Image(2, cloudRenderRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                  .Image(6, cloudNoise_->GetBaseTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(7, cloudNoise_->GetDetailTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(8, cloudNoise_->GetWeatherTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(9, cloudNoise_->GetCurlTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(10, AtmoLUTs::Get().GetTransmittanceLUT()->GetTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(11, AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveRange()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(14, cloudDepthRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                  .CombinedImageSampler(20, AtmoLUTs::Get().GetSkyViewLUT()->GetTexture()->GetView(), AtmoLUTs::Get().GetSkyViewLUT()->GetTexture()->GetSampler(), 
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Image(22, cloudFogRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                  .Buffer(21, cloudUBO_->GetBuffer())
                                  .CombinedImageSampler(27, AtmoLUTs::Get().GetMultiScatterLUT()->GetTexture()->GetView(),
                                                        SharedSamplers::GetLinearClampSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                  .Buffer(29, atmoUBO_->GetBuffer())
                                  .Build();
        raymarchWrites.Apply();
        BindSharedCameraData(31, 1, raymarchSet_.get());

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            // --- Reconstruct set: only static bindings ---
            auto reconstructWrites = DescriptorSetWriteBuilder(*reconstructSet_[i])
                                         .Image(3, cloudRenderRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Image(15, cloudDepthRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Image(23, cloudFogRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Buffer(21, cloudUBO_->GetBuffer())
                                         .Buffer(29, atmoUBO_->GetBuffer())
                                         // Initialize write targets with GENERAL layout
                                         .Image(12, reconColor_[i]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                         .Image(16, reconDepth_[i]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                         .Image(24, reconFog_[i]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                         // Initialize history with dummy texture
                                         .Image(18, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Image(19, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Image(26, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         .Build();
            reconstructWrites.Apply();
            BindSharedCameraData(31, 1, reconstructSet_[i].get());

            // --- Composite set: only static bindings ---
            auto compositeWrites = DescriptorSetWriteBuilder(*compositeSet_[i])
                                       .Image(10, AtmoLUTs::Get().GetTransmittanceLUT()->GetTexture()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Buffer(21, cloudUBO_->GetBuffer())
                                       .CombinedImageSampler(27, AtmoLUTs::Get().GetMultiScatterLUT()->GetTexture()->GetView(),
                                                             SharedSamplers::GetLinearClampSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Buffer(29, atmoUBO_->GetBuffer())
                                       .Build();
            compositeWrites.Apply();
            BindSharedCameraData(31, 1, compositeSet_[i].get());
        }
    }

    void CloudPipelinePass::SetFrameData(const Mat4 &invProj,
                                         const Mat4 &invView,
                                         const Vec3 &cameraPos,
                                         const Vec3 &planetPos,
                                         const Vec3 &sunDir,
                                         Vec2 screenSize)
    {
        const Vec3 sd = glm::length(sunDir) > 1e-6f
                            ? normalize(sunDir)
                            : Vec3(0.577f, 0.577f, 0.577f);
        cachedSunDir_ = sd;

        const float ruToM = data_.params.bottomRadius / data_.params.renderUnitRadius;
        const Vec3 pos = (cameraPos - planetPos) * ruToM;

        data_.ubo.invProj = invProj;
        data_.ubo.invView = invView;
        data_.ubo.cameraPos = Vec4(pos, 0.0f);
        data_.ubo.planetPos = Vec4(0.0f);
        data_.ubo.sunDir = Vec4(sd, data_.params.sunIntensity);
        data_.ubo.bottomRadius = data_.params.bottomRadius;
        data_.ubo.topRadius = data_.params.topRadius;
        data_.ubo.renderUnitRadius = data_.params.renderUnitRadius;
        data_.ubo.screenSize = screenSize;
        data_.ubo.sunCol = Vec4(1);
    }

    void CloudPipelinePass::PreRender(const CommandBuffer &cmd)
    {
        if (!enabled)
            return;

        auto *rs = RenderSystem::Get();
        auto *depthDesc = rs->GetAttachment("gbuf_depth");
        auto *colorDesc = rs->GetAttachment("hdr");
        auto *depthImg = dynamic_cast<const ImageDepth *>(depthDesc);
        auto *colorImg = dynamic_cast<const Image2d *>(colorDesc);
        if (!depthImg || !colorImg)
            return;

        const uint32_t cur = frameSlot_ % kFramesInFlight;
        const uint32_t hist = (frameSlot_ + kFramesInFlight - 1) % kFramesInFlight;
        const bool hasHistory = (framesSinceStart_ > 0);

        // ===================================================================
        // Transition scene color to GENERAL for compute write
        // ===================================================================
        {
            VkImageLayout currentColorLayout = colorImg->GetLayout();
            if (currentColorLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            {
                Image::InsertImageMemoryBarrier(cmd, const_cast<Image2d *>(colorImg)->GetImage(),
                                                0, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            }
            else
            {
                Image::InsertImageMemoryBarrier(cmd, const_cast<Image2d *>(colorImg)->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT,
                                                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            }
        }

        // ===================================================================
        // Update composite set: scene color/depth targets
        // ===================================================================
        {
            VkDescriptorImageInfo colorInfo{};
            colorInfo.sampler = VK_NULL_HANDLE;
            colorInfo.imageView = colorImg->GetView();
            colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo depthInfo{};
            depthInfo.sampler = VK_NULL_HANDLE;
            depthInfo.imageView = depthImg->GetView();
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = compositeSet_[cur]->GetDescriptorSet();
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[0].pImageInfo = &colorInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = compositeSet_[cur]->GetDescriptorSet();
            writes[1].dstBinding = 4;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[1].pImageInfo = &depthInfo;

            DescriptorSet::Update({writes[0], writes[1]});
        }

        totalTime_ += 0.016f;
        atmoUBO_->Update(data_.ubo);
        UpdateCloudUBO();

        auto qext = cloudRenderRT_->GetExtent();
        auto fext = colorImg->GetExtent();

        // ===================================================================
        // Raymarch pass
        // ===================================================================
        {
            Image2d *quarterImages[3] = {cloudRenderRT_.get(), cloudDepthRT_.get(), cloudFogRT_.get()};

            // Pre-write barriers: transition to GENERAL
            for (int i = 0; i < 3; ++i)
            {
                VkImageLayout currentLayout = quarterImages[i]->GetLayout();

                Image::InsertImageMemoryBarrier(cmd, quarterImages[i]->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT,
                                                VK_ACCESS_SHADER_WRITE_BIT,
                                                currentLayout,
                                                VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                quarterImages[i]->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
            }

            // Dispatch raymarch
            raymarchPipeline_->BindPipeline(cmd);
            raymarchSet_->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, raymarchPipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            raymarchPipeline_->CmdRender(cmd, UVec2(qext.x, qext.y), /*LOCAL_X=*/8, /*LOCAL_Y=*/8, /*LOCAL_Z=*/1);

            // Post-write barriers: transition to SHADER_READ_ONLY_OPTIMAL
            for (int i = 0; i < 3; ++i)
            {
                Image::InsertImageMemoryBarrier(cmd, quarterImages[i]->GetImage(),
                                                VK_ACCESS_SHADER_WRITE_BIT,
                                                VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                quarterImages[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            // Global memory barrier to ensure writes are visible
            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        // ===================================================================
        // Transition reconstruction targets to GENERAL for writing
        // ===================================================================
        {
            Image2d *reconTargets[3] = {reconColor_[cur].get(), reconDepth_[cur].get(), reconFog_[cur].get()};
            for (int i = 0; i < 3; ++i)
            {
                VkImageLayout currentLayout = reconTargets[i]->GetLayout();

                Image::InsertImageMemoryBarrier(cmd, reconTargets[i]->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT,
                                                VK_ACCESS_SHADER_WRITE_BIT,
                                                currentLayout,
                                                VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                reconTargets[i]->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
            }
        }

        // ===================================================================
        // Update reconstruction set descriptor
        // ===================================================================
        {
            VkDescriptorImageInfo imageInfos[9]{};
            VkDescriptorBufferInfo bufferInfos[2]{};
            VkWriteDescriptorSet writes[11]{};
            uint32_t writeCount = 0;

            auto addImageWrite = [&](uint32_t binding, VkImageView view, VkImageLayout layout, VkDescriptorType type)
            {
                imageInfos[writeCount].sampler = VK_NULL_HANDLE;
                imageInfos[writeCount].imageView = view;
                imageInfos[writeCount].imageLayout = layout;

                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].dstSet = reconstructSet_[cur]->GetDescriptorSet();
                writes[writeCount].dstBinding = binding;
                writes[writeCount].descriptorCount = 1;
                writes[writeCount].descriptorType = type;
                writes[writeCount].pImageInfo = &imageInfos[writeCount];
                writeCount++;
            };

            auto addBufferWrite = [&](uint32_t binding, VkBuffer buffer)
            {
                uint32_t bufIdx = writeCount - 9;
                bufferInfos[bufIdx].buffer = buffer;
                bufferInfos[bufIdx].offset = 0;
                bufferInfos[bufIdx].range = VK_WHOLE_SIZE;

                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].dstSet = reconstructSet_[cur]->GetDescriptorSet();
                writes[writeCount].dstBinding = binding;
                writes[writeCount].descriptorCount = 1;
                writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[writeCount].pBufferInfo = &bufferInfos[bufIdx];
                writeCount++;
            };

            // Static read-only inputs (quarter-res targets are in SHADER_READ_ONLY_OPTIMAL)
            addImageWrite(3, cloudRenderRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            addImageWrite(15, cloudDepthRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            addImageWrite(23, cloudFogRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

            // Current frame write targets (in GENERAL layout)
            addImageWrite(12, reconColor_[cur]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            addImageWrite(16, reconDepth_[cur]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            addImageWrite(24, reconFog_[cur]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

            // History read bindings
            if (hasHistory)
            {
                // Valid history: use previous frame's reconstruction outputs
                addImageWrite(18, reconColor_[hist]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                addImageWrite(19, reconDepth_[hist]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                addImageWrite(26, reconFog_[hist]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            }
            else
            {
                // Frame 0: use dummy texture to avoid layout conflict
                // (current frame's images are in GENERAL, can't be read as SHADER_READ_ONLY_OPTIMAL)
                addImageWrite(18, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                addImageWrite(19, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                addImageWrite(26, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            }

            // Uniform buffers
            addBufferWrite(21, cloudUBO_->GetBuffer());
            addBufferWrite(29, atmoUBO_->GetBuffer());

            DescriptorSet::Update(std::vector<VkWriteDescriptorSet>(writes, writes + writeCount));
        }

        // ===================================================================
        // Update composite set to read reconstruction outputs
        // ===================================================================
        {
            VkDescriptorImageInfo imageInfos[2]{};

            imageInfos[0].sampler = VK_NULL_HANDLE;
            imageInfos[0].imageView = reconColor_[cur]->GetView();
            imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageInfos[1].sampler = VK_NULL_HANDLE;
            imageInfos[1].imageView = reconFog_[cur]->GetView();
            imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = compositeSet_[cur]->GetDescriptorSet();
            writes[0].dstBinding = 13;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[0].pImageInfo = &imageInfos[0];

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = compositeSet_[cur]->GetDescriptorSet();
            writes[1].dstBinding = 25;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[1].pImageInfo = &imageInfos[1];

            DescriptorSet::Update({writes[0], writes[1]});
        }

        // ===================================================================
        // Reconstruct pass
        // ===================================================================
        reconstructPipeline_->BindPipeline(cmd);
        reconstructSet_[cur]->BindDescriptor(cmd);
        SharedSamplers::BindSharedSamplerSet(cmd, reconstructPipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
        reconstructPipeline_->CmdRender(cmd, UVec3(fext.x, fext.y, fext.z), /*LOCAL_X=*/8, /*LOCAL_Y=*/8, /*LOCAL_Z=*/1);

        // Transition reconstruction outputs to SHADER_READ_ONLY_OPTIMAL
        {
            Image2d *reconImages[3] = {reconColor_[cur].get(), reconDepth_[cur].get(), reconFog_[cur].get()};
            for (int i = 0; i < 3; ++i)
            {
                Image::InsertImageMemoryBarrier(cmd, reconImages[i]->GetImage(),
                                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                reconImages[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ===================================================================
        // Composite pass
        // ===================================================================
        compositePipeline_->BindPipeline(cmd);
        compositeSet_[cur]->BindDescriptor(cmd);
        SharedSamplers::BindSharedSamplerSet(cmd, compositePipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
        compositePipeline_->CmdRender(cmd, UVec2(fext.x, fext.y), /*LOCAL_X=*/8, /*LOCAL_Y=*/8, /*LOCAL_Z=*/1);

        // Transition scene color back to SHADER_READ_ONLY_OPTIMAL
        Image::InsertImageMemoryBarrier(cmd, const_cast<Image2d *>(colorImg)->GetImage(),
                                        VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_ACCESS_SHADER_READ_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
        const_cast<Image2d *>(colorImg)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ++frameSlot_;
        ++framesSinceStart_;
    }

    void CloudPipelinePass::Render(const CommandBuffer &cmd)
    {
    }

    void CloudPipelinePass::UpdateCloudUBO()
    {
        CloudUBO ubo{};

        ubo.cloudBottomRadius = data_.params.bottomRadius + minAlt;
        ubo.cloudTopRadius = data_.params.bottomRadius + maxAlt;
        ubo.stepCount = static_cast<float>(marchSteps);
        ubo.lightStepCount = static_cast<float>(lightMarchSteps);

        ubo.cloudDensityScale = densityScale;
        ubo.cloudCoverage = coverage;
        ubo.cloudDetailScale = cloudDetailScale;
        ubo.cloudCurlNoiseScale = cloudCurlNoiseScale;
        ubo.cloudBaseNoiseScale = cloudBaseNoiseScale;
        ubo.cloudWeatherUVScale = cloudWeatherUVScale;

        ubo.time = totalTime_;
        ubo.Wind = Wind;
        ubo.Speed = Speed;
        ubo.unused = unused;

        frameCounter_ = (frameCounter_ + 1) % 256;
        ubo.frameIndex = static_cast<int>(frameCounter_);

        cloudUBO_->Update(ubo);
    }

    void CloudPipelinePass::DrawImGuiPanel()
    {
        if (!isWindowOpen)
            return;

        ImGui::Begin("Cloud Debug", &isWindowOpen);
        ImGui::Checkbox("Enabled", &enabled);
        ImGui::SliderFloat("Min Altitude", &minAlt, 1000.0f, 50000.0f);
        ImGui::SliderFloat("Max Altitude", &maxAlt, 1000.0f, 50000.0f);
        ImGui::SliderFloat("Density Scale", &densityScale, 0.0f, 10.0f);
        ImGui::SliderFloat("Coverage", &coverage, 0.0f, 1.0f);
        ImGui::SliderInt("March Steps", &marchSteps, 4, 128);
        ImGui::SliderInt("Light March Steps", &lightMarchSteps, 2, 32);
        ImGui::SliderFloat("Base Noise Scale", &cloudBaseNoiseScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Detail Scale", &cloudDetailScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Curl Noise Scale", &cloudCurlNoiseScale, 0.0f, 5.0f);
        ImGui::SliderFloat("Weather UV Scale", &cloudWeatherUVScale, 0.1f, 10.0f);
        ImGui::End();
    }

} // namespace SF::Engine