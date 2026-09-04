#include "AtmospherePipelinePass.hpp"
#include <Rendering/RenderSystem.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Descriptors/DescriptorSetBuilder.hpp>
#include <Rendering/SharedFunctions.hpp>
#include <Rendering/SharedSamplers.hpp>

namespace SF::Engine
{
    AtmospherePipelinePass::AtmospherePipelinePass(Pipeline::Stage stage,
                                                   const AtmosphereParams &params)
        : PipelinePass(stage), params_(params)
    {
        PipelinePass::SetOrder(0);
        vkDeviceWaitIdle(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice());
        ubo_ = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));

        // NOTE: entry point renamed vertex/fragment "atmo_vs"/"atmo_fs" -> compute
        // "atmo_cs" now that Atmosphere.shader is a [numthreads(8,8,1)] kernel.
        pipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Atmosphere/Atmosphere.shader",
            std::vector<Shader::Define>{});

        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        // see this is so much cleaner than the bunch of manual assembly stuff
        auto Writes = DescriptorSetWriteBuilder(*descSet_)
                          .Buffer(0, ubo_->GetBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                          .CombinedImageSampler(1, AtmoLUTs::Get().GetTransmittanceLUT()->GetTexture()->GetView(),
                                                AtmoLUTs::Get().GetTransmittanceLUT()->GetTexture()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          // .CombinedImageSampler(2, AtmoLUTs::Get().GetMultiScatterLUT()->GetTexture()->GetView(), AtmoLUTs::Get().GetMultiScatterLUT()->GetTexture()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .CombinedImageSampler(2, AtmoLUTs::Get().GetSkyViewLUT()->GetTexture()->GetView(),
                                                AtmoLUTs::Get().GetSkyViewLUT()->GetTexture()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .CombinedImageSampler(3, AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveColorRGBTransR()->GetView(),
                                                AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveColorRGBTransR()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .CombinedImageSampler(4, AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveTransGB()->GetView(),
                                                AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveTransGB()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .CombinedImageSampler(5, AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveRange()->GetView(),
                                                AtmoLUTs::Get().GetAerialPerspectiveLUT()->GetAerialPerspectiveRange()->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                          .Build();

        Writes.Apply();
    }

    void AtmospherePipelinePass::SetSceneBuffers()
    {
        const Image2d *sceneColor = GetSceneHDR(); // "hdr" attachment; now our compute read/write target
        const ImageDepth *sceneDepth = GetSceneDepth();

        if (!sceneColor || !sceneDepth)
            return;

        // Avoid redundant descriptor writes if nothing changed.
        if (sceneColor == lastColor_ && sceneDepth == lastDepth_)
            return;
        lastColor_ = sceneColor;
        lastDepth_ = sceneDepth;

        assert(sceneColor != nullptr);
        auto Writes = DescriptorSetWriteBuilder(*descSet_)
                          .CombinedImageSampler(6, sceneDepth->GetView(), sceneDepth->GetSampler(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
                          .Image(7, sceneColor->GetView(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                          .Build();

        Writes.Apply();
    }

    void AtmospherePipelinePass::SetFrameData(const Mat4 &invProj,
                                              const Mat4 &invView,
                                              const Vec3 &cameraPos,
                                              const Vec3 &planetPos,
                                              const Vec3 &sunDir,
                                              Vec2 screenSize)
    {
        const Vec3 sd = glm::length(sunDir) > 1e-6f
                            ? normalize(sunDir)
                            : Vec3(0.577f, 0.577f, 0.577f);

        const float ruToM = params_.bottomRadius / params_.renderUnitRadius;
        Vec3 viewPosSI = Vec3(
            (glm::dvec3(cameraPos) - glm::dvec3(planetPos)) * (double)ruToM);

        frameData_.invProj = invProj;
        frameData_.invView = invView;
        frameData_.cameraPos = Vec4(viewPosSI, 0.0f);
        frameData_.planetPos = Vec4(0.0f);
        frameData_.sunDir = Vec4(sd, params_.sunIntensity);
        frameData_.bottomRadius = params_.bottomRadius;
        frameData_.topRadius = params_.topRadius;
        frameData_.renderUnitRadius = params_.renderUnitRadius;
        frameData_.screenSize = screenSize;
        frameData_.sunCol = Vec3(1);

        SkyViewPushConstants svp{};
        svp.sunDir = Vec4(sd, params_.sunIntensity);
        svp.camPos = Vec4(viewPosSI, 0);
        svp.bottomRadius = params_.bottomRadius;
        svp.topRadius = params_.topRadius;
        svp.pad0_ = Vec4(0);
        AtmoLUTs::Get().GetSkyViewLUT()->SetParams(svp);
    }

    namespace
    {
        // hdr comes out of the opaque pass in COLOR_ATTACHMENT_OPTIMAL (or
        // whatever RenderStage transitions it to once that render pass ends).
        // A compute shader can only read/write it as a storage image while
        // it's in GENERAL, so we flip it there for the dispatch and flip it
        // back to SHADER_READ_ONLY_OPTIMAL afterwards for whatever consumes
        // "hdr" next (tonemap / blit-to-swapchain / etc).
        //
        // NOTE: this assumes Image2d exposes GetImage() (raw VkImage) the same
        // way it already exposes GetSampler()/GetView(); adjust if the real
        // accessor is named differently.
        void TransitionHdrLayout(const CommandBuffer &commandBuffer, const Image2d *hdr,
                                 VkImageLayout oldLayout, VkImageLayout newLayout,
                                 VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = const_cast<Image2d *>(hdr)->GetImage();
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.srcAccessMask = srcAccess;
            barrier.dstAccessMask = dstAccess;

            vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    void AtmospherePipelinePass::PreRender(const CommandBuffer &commandBuffer)
    {
        // Must run BEFORE the lastColor_ guard below; lastColor_ is only ever
        // populated as a side effect of this call. Checking the guard first
        // (as in an earlier draft) meant lastColor_ could never get set on the
        // first frame, and the pass would silently no-op forever after that.
        SetSceneBuffers();

        if (!lastColor_)
            return; // scene attachments not available yet (e.g. before first frame)

        ubo_->Update(frameData_);

        AtmoLUTs::Get().GetSkyViewLUT()->Bake(commandBuffer);
        AtmoLUTs::Get().GetAerialPerspectiveLUT()->Bake(commandBuffer, frameData_);

        TransitionHdrLayout(commandBuffer, lastColor_,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_->GetPipeline());
        descSet_->BindDescriptor(commandBuffer);

        UVec3 extent{static_cast<uint32_t>(frameData_.screenSize.x),
                     static_cast<uint32_t>(frameData_.screenSize.y),
                     1u};
        pipeline_->CmdRender(commandBuffer, extent, /*LOCAL_X=*/8, /*LOCAL_Y=*/8, /*LOCAL_Z=*/1);

        TransitionHdrLayout(commandBuffer, lastColor_,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void AtmospherePipelinePass::Render(const CommandBuffer &commandBuffer)
    {
    }
}