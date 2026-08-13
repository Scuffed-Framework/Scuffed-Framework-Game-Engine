#include "SSRPipelinePass.hpp"
#include <Rendering/RenderSystem.hpp>
#include <Rendering/SharedFunctions.hpp>
#include <Rendering/SharedSamplers.hpp>
#include <Rendering/Descriptors/DescriptorSetBuilder.hpp>
#include <Gui/ocornut/imgui.h>

namespace SF::Engine
{
    bool SSRPipelinePass::isWindowOpen = true;

    SSRPipelinePass::SSRPipelinePass(Pipeline::Stage stage, LightManager &lightManager)
        : PipelinePass(stage), lm_(lightManager)
    {
        // Run after the deferred-light subpass (writes hdr) and before
        // forward-transparent — see LightingRenderer.hpp registration.
        PipelinePass::SetOrder(50);

        uiHandle_ = UIRegistry::Get().Register([this]
                                               { DrawImGuiPanel(); });

        ssrUBO_ = std::make_unique<UniformBuffer>(sizeof(SSRParams));

        CreateResources();
        CreatePipelines();
        BindStaticDescriptors();

        isWindowOpen = true;
    }

    void SSRPipelinePass::CreateResources()
    {
        UVec2 fullRes = UVec2(static_cast<uint32_t>(GetScreenSize().x),
                              static_cast<uint32_t>(GetScreenSize().y));

        auto makeRT = [&](const UVec2 &res)
        {
            return std::make_unique<Image2d>(res, VK_FORMAT_R16G16B16A16_SFLOAT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        };

        rayDirRT_ = makeRT(fullRes);
        rayDataRT_ = makeRT(fullRes);
        traceColorRT_ = makeRT(fullRes);
        traceHitRT_ = makeRT(fullRes);
        filteredRT_ = makeRT(fullRes);

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            accumColor_[i] = makeRT(fullRes);
            accumMoments_[i] = makeRT(fullRes);
        }

        // 1x1 dummy history for frame 0 (no valid previous-frame accumulation yet).
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
            clearColor.float32[3] = 0.0f;
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

        // All scratch/history targets are created in GENERAL (constructor
        // already transitions UNDEFINED -> GENERAL); flip them to
        // SHADER_READ_ONLY_OPTIMAL up front so the very first BindStaticDescriptors()
        // call can wire them as sampled inputs without a layout mismatch —
        // PreRender() flips each back to GENERAL right before it's written.
        CommandBuffer cmd(true);
        auto toReadOnly = [&](Image2d *img)
        {
            Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                            0, VK_ACCESS_SHADER_READ_BIT,
                                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
        };
        toReadOnly(rayDirRT_.get());
        toReadOnly(rayDataRT_.get());
        toReadOnly(traceColorRT_.get());
        toReadOnly(traceHitRT_.get());
        toReadOnly(filteredRT_.get());
        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            toReadOnly(accumColor_[i].get());
            toReadOnly(accumMoments_[i].get());
        }
        cmd.SubmitIdle();

        rayDirRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rayDataRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        traceColorRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        traceHitRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        filteredRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            accumColor_[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            accumMoments_[i]->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    void SSRPipelinePass::CreatePipelines()
    {
        rayGenPipeline_ = std::make_unique<ComputePipeline>("Shaders/SSR/RayGen.shader");
        tracePipeline_ = std::make_unique<ComputePipeline>("Shaders/SSR/Trace.shader");
        temporalPipeline_ = std::make_unique<ComputePipeline>("Shaders/SSR/TemporalAccumulate.shader");
        spatialPipeline_ = std::make_unique<ComputePipeline>("Shaders/SSR/SpatialFilter.shader");
        compositePipeline_ = std::make_unique<ComputePipeline>("Shaders/SSR/Composite.shader");

        rayGenSet_ = std::make_unique<DescriptorSet>(*rayGenPipeline_);
        traceSet_ = std::make_unique<DescriptorSet>(*tracePipeline_);
        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            temporalSet_[i] = std::make_unique<DescriptorSet>(*temporalPipeline_);
            spatialSet_[i] = std::make_unique<DescriptorSet>(*spatialPipeline_);
            compositeSet_[i] = std::make_unique<DescriptorSet>(*compositePipeline_);
        }
    }

    void SSRPipelinePass::BindStaticDescriptors()
    {
        // --- RayGen : static bindings only (gbuffer is rewritten per-frame
        // in PreRender since attachment pointers can change on resize). ---
        auto rayGenWrites = DescriptorSetWriteBuilder(*rayGenSet_)
                                .Buffer(0, ssrUBO_->GetBuffer())
                                .Image(4, rayDirRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                .Image(5, rayDataRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                .Build();
        rayGenWrites.Apply();
        BindSharedCameraData(kSSRCameraBind, 1, rayGenSet_.get());

        // --- Trace : static bindings only. ---
        auto traceWrites = DescriptorSetWriteBuilder(*traceSet_)
                               .Buffer(0, ssrUBO_->GetBuffer())
                               .Image(4, rayDirRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                               .Image(5, rayDataRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                               .Image(6, traceColorRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                               .Image(7, traceHitRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                               .Build();
        traceWrites.Apply();
        BindSharedCameraData(kSSRCameraBind, 1, traceSet_.get());

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            // --- TemporalAccumulate[i] : static bindings; history read
            // bindings (5/6) are (re)pointed at the previous slot every
            // frame in PreRender, seeded with the dummy texture here. ---
            auto temporalWrites = DescriptorSetWriteBuilder(*temporalSet_[i])
                                      .Buffer(0, ssrUBO_->GetBuffer())
                                      .Image(3, traceColorRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                      .Image(4, traceHitRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                      .Image(5, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                      .Image(6, dummyTexture_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                      .Image(7, accumColor_[i]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                      .Image(8, accumMoments_[i]->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                      .Build();
            temporalWrites.Apply();
            BindSharedCameraData(kSSRCameraBind, 1, temporalSet_[i].get());

            // --- SpatialFilter[i] : reads accumColor_[i]/accumMoments_[i]
            // (this frame's temporal output — no ping-pong needed on the
            // read side since spatial filtering doesn't carry state
            // across frames), writes filteredRT_. ---
            auto spatialWrites = DescriptorSetWriteBuilder(*spatialSet_[i])
                                     .Buffer(0, ssrUBO_->GetBuffer())
                                     .Image(4, accumColor_[i]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                     .Image(5, accumMoments_[i]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                     .Image(6, filteredRT_->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                     .Build();
            spatialWrites.Apply();
            BindSharedCameraData(kSSRCameraBind, 1, spatialSet_[i].get());

            // --- Composite[i] : gbuffer/hdr bindings rewritten per-frame
            // in PreRender (cross-stage attachments). ---
            auto compositeWrites = DescriptorSetWriteBuilder(*compositeSet_[i])
                                       .Buffer(0, ssrUBO_->GetBuffer())
                                       .Image(5, filteredRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Image(6, rayDirRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Image(7, traceColorRT_->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Image(8, accumColor_[i]->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                       .Build();
            compositeWrites.Apply();
            BindSharedCameraData(kSSRCameraBind, 1, compositeSet_[i].get());
        }
    }

    void SSRPipelinePass::UpdateUBO()
    {
        SSRParams p{};
        p.screenSize = GetScreenSize();
        p.invScreenSize = Vec2(1.0f / p.screenSize.x, 1.0f / p.screenSize.y);

        p.maxSteps = maxSteps;
        p.thickness = thickness;
        p.strideScale = strideScale;
        frameCounter_ = (frameCounter_ + 1) % 65536u;
        p.frameIndex = static_cast<int32_t>(frameCounter_);

        p.maxRoughness = maxRoughness;
        p.intensity = intensity;
        p.temporalBlendMin = temporalBlendMin;
        p.temporalBlendMax = temporalBlendMax;

        p.spatialRadiusPx = spatialRadiusPx;
        p.varianceClampGamma = varianceClampGamma;
        p.debugView = static_cast<int32_t>(debugView);
        p.binarySearchSteps = binarySearchSteps;

        p.ambientSkyColor = ambientSkyColor;
        p.ambientIntensity = ambientIntensity;
        p.ambientGroundColor = ambientGroundColor;
        p.edgeFadeStart = edgeFadeStart;

        p.bTemporalEnabled = temporalEnabled ? 1 : 0;
        p.bSpatialEnabled = spatialEnabled ? 1 : 0;
        p.bProbeFallbackEnabled = probeFallbackEnabled ? 1 : 0;
        p.depthBufferThicknessBias = depthBufferThicknessBias;

        ssrUBO_->Update(p);
    }

    void SSRPipelinePass::PreRender(const CommandBuffer &cmd)
    {
        if (!enabled)
            return;

        auto *rs = RenderSystem::Get();
        auto *depthImg = dynamic_cast<const ImageDepth *>(rs->GetAttachment("gbuf_depth"));
        auto *normalImg = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_normal"));
        auto *albedoImg = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_albedo"));
        auto *pbrImg = dynamic_cast<const Image2d *>(rs->GetAttachment("gbuf_pbr"));
        auto *colorImg = dynamic_cast<const Image2d *>(rs->GetAttachment("hdr"));
        if (!depthImg || !normalImg || !albedoImg || !pbrImg || !colorImg)
            return;

        UpdateUBO();

        const uint32_t cur = frameSlot_ % kFramesInFlight;
        const uint32_t hist = (frameSlot_ + kFramesInFlight - 1) % kFramesInFlight;
        const bool hasHistory = (framesSinceStart_ > 0);

        // ===================================================================
        // Transition hdr to GENERAL : Trace samples it, Composite read-
        // modify-writes it via RWTexture2D. Same dance CloudPipelinePass
        // does for the same attachment.
        // ===================================================================
        {
            VkImageLayout cur_layout = colorImg->GetLayout();
            VkAccessFlags srcAccess = (cur_layout == VK_IMAGE_LAYOUT_UNDEFINED) ? 0 : VK_ACCESS_SHADER_READ_BIT;
            VkImageLayout srcLayout = (cur_layout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            Image::InsertImageMemoryBarrier(cmd, const_cast<Image2d *>(colorImg)->GetImage(),
                                            srcAccess, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                            srcLayout, VK_IMAGE_LAYOUT_GENERAL,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            const_cast<Image2d *>(colorImg)->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
        }

        auto ext = colorImg->GetExtent();
        UVec2 full{ext.x, ext.y};

        // ===================================================================
        // RayGen : rewrite gbuffer reads (attachment pointers can change on
        // resize), transition rayDir/rayData to GENERAL, dispatch.
        // ===================================================================
        {
            VkDescriptorImageInfo depthII{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo normalII{VK_NULL_HANDLE, normalImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo pbrII{VK_NULL_HANDLE, pbrImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet writes[3]{};
            VkDescriptorImageInfo infos[3] = {depthII, normalII, pbrII};
            uint32_t bindings[3] = {1, 2, 3};
            for (int i = 0; i < 3; ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = rayGenSet_->GetDescriptorSet();
                writes[i].dstBinding = bindings[i];
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writes[i].pImageInfo = &infos[i];
            }
            DescriptorSet::Update({writes[0], writes[1], writes[2]});

            Image2d *writeTargets[2] = {rayDirRT_.get(), rayDataRT_.get()};
            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                img->GetLayout(), VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
            }

            rayGenPipeline_->BindPipeline(cmd);
            rayGenSet_->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, rayGenPipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            rayGenPipeline_->CmdRender(cmd, full, 8, 8, 1);

            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ===================================================================
        // Trace (+ probe fallback on miss)
        // ===================================================================
        {
            VkDescriptorImageInfo depthII{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo hdrII{VK_NULL_HANDLE, colorImg->GetView(), VK_IMAGE_LAYOUT_GENERAL};

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = traceSet_->GetDescriptorSet();
            writes[0].dstBinding = 1;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[0].pImageInfo = &depthII;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = traceSet_->GetDescriptorSet();
            writes[1].dstBinding = 3;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[1].pImageInfo = &hdrII;
            DescriptorSet::Update({writes[0], writes[1]});

            Image2d *writeTargets[2] = {traceColorRT_.get(), traceHitRT_.get()};
            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                img->GetLayout(), VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
            }

            tracePipeline_->BindPipeline(cmd);
            traceSet_->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, tracePipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            tracePipeline_->CmdRender(cmd, full, 8, 8, 1);

            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ===================================================================
        // TemporalAccumulate[cur] : point the history bindings at slot
        // `hist` (or the dummy texture on the very first frame), rewrite
        // gbuf_depth, transition accumColor_[cur]/accumMoments_[cur] to
        // GENERAL, dispatch.
        // ===================================================================
        {
            VkDescriptorImageInfo depthII{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo histColorII{VK_NULL_HANDLE,
                                              hasHistory ? accumColor_[hist]->GetView() : dummyTexture_->GetView(),
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo histMomentsII{VK_NULL_HANDLE,
                                                hasHistory ? accumMoments_[hist]->GetView() : dummyTexture_->GetView(),
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet writes[3]{};
            VkDescriptorImageInfo infos[3] = {depthII, histColorII, histMomentsII};
            uint32_t bindings[3] = {1, 5, 6};
            for (int i = 0; i < 3; ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = temporalSet_[cur]->GetDescriptorSet();
                writes[i].dstBinding = bindings[i];
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writes[i].pImageInfo = &infos[i];
            }
            DescriptorSet::Update({writes[0], writes[1], writes[2]});

            Image2d *writeTargets[2] = {accumColor_[cur].get(), accumMoments_[cur].get()};
            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                img->GetLayout(), VK_IMAGE_LAYOUT_GENERAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_GENERAL);
            }

            temporalPipeline_->BindPipeline(cmd);
            temporalSet_[cur]->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, temporalPipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            temporalPipeline_->CmdRender(cmd, full, 8, 8, 1);

            for (auto *img : writeTargets)
            {
                Image::InsertImageMemoryBarrier(cmd, img->GetImage(),
                                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
                img->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ===================================================================
        // SpatialFilter[cur] : reads accumColor_[cur]/accumMoments_[cur]
        // (already static-bound in BindStaticDescriptors — no rewrite
        // needed besides gbuffer), writes filteredRT_.
        // ===================================================================
        {
            VkDescriptorImageInfo depthII{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo normalII{VK_NULL_HANDLE, normalImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo pbrII{VK_NULL_HANDLE, pbrImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet writes[3]{};
            VkDescriptorImageInfo infos[3] = {depthII, normalII, pbrII};
            uint32_t bindings[3] = {1, 2, 3};
            for (int i = 0; i < 3; ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = spatialSet_[cur]->GetDescriptorSet();
                writes[i].dstBinding = bindings[i];
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writes[i].pImageInfo = &infos[i];
            }
            DescriptorSet::Update({writes[0], writes[1], writes[2]});

            Image::InsertImageMemoryBarrier(cmd, filteredRT_->GetImage(),
                                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                            filteredRT_->GetLayout(), VK_IMAGE_LAYOUT_GENERAL,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            filteredRT_->SetLayout(VK_IMAGE_LAYOUT_GENERAL);

            spatialPipeline_->BindPipeline(cmd);
            spatialSet_[cur]->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, spatialPipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            spatialPipeline_->CmdRender(cmd, full, 8, 8, 1);

            Image::InsertImageMemoryBarrier(cmd, filteredRT_->GetImage(),
                                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
            filteredRT_->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // ===================================================================
        // Composite[cur] : rewrite gbuffer + hdr bindings, dispatch
        // straight into hdr (RWTexture2D read-modify-write).
        // ===================================================================
        {
            VkDescriptorImageInfo depthII{VK_NULL_HANDLE, depthImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo normalII{VK_NULL_HANDLE, normalImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo albedoII{VK_NULL_HANDLE, albedoImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo pbrII{VK_NULL_HANDLE, pbrImg->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo hdrII{VK_NULL_HANDLE, colorImg->GetView(), VK_IMAGE_LAYOUT_GENERAL};

            VkWriteDescriptorSet writes[5]{};
            VkDescriptorImageInfo infos[5] = {depthII, normalII, albedoII, pbrII, hdrII};
            uint32_t bindings[5] = {1, 2, 3, 4, 9};
            VkDescriptorType types[5] = {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
            for (int i = 0; i < 5; ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = compositeSet_[cur]->GetDescriptorSet();
                writes[i].dstBinding = bindings[i];
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = types[i];
                writes[i].pImageInfo = &infos[i];
            }
            DescriptorSet::Update({writes[0], writes[1], writes[2], writes[3], writes[4]});

            compositePipeline_->BindPipeline(cmd);
            compositeSet_[cur]->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, compositePipeline_->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
            compositePipeline_->CmdRender(cmd, full, 8, 8, 1);
        }

        // Transition hdr back to SHADER_READ_ONLY_OPTIMAL for the
        // forward-transparent and tonemap subpasses that follow it.
        Image::InsertImageMemoryBarrier(cmd, const_cast<Image2d *>(colorImg)->GetImage(),
                                        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
        const_cast<Image2d *>(colorImg)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ++frameSlot_;
        ++framesSinceStart_;
    }

    void SSRPipelinePass::DrawImGuiPanel()
    {
        if (!isWindowOpen)
            return;

        ImGui::Begin("SSR Debug", &isWindowOpen);
        ImGui::Checkbox("Enabled", &enabled);
        ImGui::Checkbox("Temporal Accumulation", &temporalEnabled);
        ImGui::Checkbox("Spatial Filter", &spatialEnabled);
        ImGui::Checkbox("Probe Fallback", &probeFallbackEnabled);
        ImGui::Separator();
        ImGui::SliderInt("Max Steps", &maxSteps, 4, 128);
        ImGui::SliderFloat("Thickness (view-space)", &thickness, 0.01f, 2.0f);
        ImGui::SliderFloat("Stride Scale", &strideScale, 0.1f, 4.0f);
        ImGui::SliderInt("Binary Search Steps", &binarySearchSteps, 0, 12);
        ImGui::SliderFloat("Max Roughness", &maxRoughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 4.0f);
        ImGui::Separator();
        ImGui::SliderFloat("Temporal Blend Min", &temporalBlendMin, 0.0f, 1.0f);
        ImGui::SliderFloat("Temporal Blend Max", &temporalBlendMax, 0.0f, 1.0f);
        ImGui::SliderFloat("Spatial Radius (px)", &spatialRadiusPx, 0.0f, 32.0f);
        ImGui::SliderFloat("Variance Clamp Gamma", &varianceClampGamma, 0.0f, 16.0f);
        ImGui::Separator();
        ImGui::ColorEdit3("Ambient Sky Color", &ambientSkyColor.x);
        ImGui::ColorEdit3("Ambient Ground Color", &ambientGroundColor.x);
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 4.0f);
        ImGui::Separator();
        const char *debugNames[] = {"None", "Ray Direction", "Trace Raw", "Temporal", "Spatial (Final)", "Confidence"};
        int dbgIdx = static_cast<int>(debugView);
        if (ImGui::Combo("Debug View", &dbgIdx, debugNames, IM_ARRAYSIZE(debugNames)))
            debugView = static_cast<SSRDebugView>(dbgIdx);
        ImGui::End();
    }
} // namespace SF::Engine
