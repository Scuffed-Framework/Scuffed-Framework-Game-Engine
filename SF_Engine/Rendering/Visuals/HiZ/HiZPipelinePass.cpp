#include "HiZPipelinePass.hpp"

#include <Rendering/RHI/Descriptors/DescriptorSetBuilder.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/SharedSamplers.hpp>

#include <algorithm>

namespace SF::Engine
{
    HiZPipelinePass::HiZPipelinePass(Pipeline::Stage stage) : EngineRenderpass(stage)
    {
        // Lowest order within its stage: runs before SSR (order 50) and anything else in {1,x}.
        EngineRenderpass::SetOrder(0);

        device_ = *RenderSystem::Get()->GetLogicalDevice();

        downsamplePipeline_ = std::make_unique<ComputePipeline>("Shaders/HiZ/HiZDownsample.shader");

        levelSets_.reserve(kMaxLevels);
        for (uint32_t i = 0; i < kMaxLevels; ++i)
            levelSets_.push_back(std::make_unique<DescriptorSet>(*downsamplePipeline_));
    }

    HiZPipelinePass::~HiZPipelinePass()
    {
        // Views must die before pyramid_ (member destruction runs after this body).
        DestroyMipViews();
    }

    void HiZPipelinePass::DestroyMipViews()
    {
        if (device_ == VK_NULL_HANDLE)
            return;
        for (VkImageView v: mipViews_)
        {
            if (v != VK_NULL_HANDLE)
                vkDestroyImageView(device_, v, nullptr);
        }
        mipViews_.clear();
    }

    void HiZPipelinePass::CreatePyramid(UVec2 pyramidRes)
    {
        // R32_SFLOAT: same precision as the 32-bit depth buffer, so no precision lost to the
        // reduction. mipmap=true allocates the full chain and moves every mip to GENERAL.
        pyramid_ = std::make_unique<Image2d>(pyramidRes, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_USAGE_STORAGE_BIT, VK_FILTER_NEAREST,
                                             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, false, true);
        pyramid_->SetLayout(VK_IMAGE_LAYOUT_GENERAL);

        levelCount_  = std::min(pyramid_->GetMipLevels(), kMaxLevels);
        allocatedRes_ = pyramidRes;
    }

    void HiZPipelinePass::CreateMipViews()
    {
        mipViews_.resize(levelCount_, VK_NULL_HANDLE);
        for (uint32_t k = 0; k < levelCount_; ++k)
        {
            Image::CreateImageView(pyramid_->GetImage(), mipViews_[k], VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_SFLOAT,
                                   VK_IMAGE_ASPECT_COLOR_BIT, 1, k, 1, 0);
        }
    }

    void HiZPipelinePass::BuildDescriptors()
    {
        for (uint32_t k = 0; k < levelCount_; ++k)
        {
            DescriptorSetWriteBuilder b(*levelSets_[k]);
            if (k > 0)
                b.Image(0, mipViews_[k - 1], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            b.Image(1, mipViews_[k], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            b.Build().Apply();
        }
        // levelSets_[0] binding 0 (gbuf_depth) is (re)written in PreRender when the depth view changes.
        lastDepthView_ = VK_NULL_HANDLE;
    }

    void HiZPipelinePass::EnsureSized(UVec2 depthRes)
    {
        if (depthRes.x == 0 || depthRes.y == 0)
            return;

        const UVec2 pyramidRes{std::max(1u, depthRes.x / 2u), std::max(1u, depthRes.y / 2u)};
        if (pyramid_ && pyramidRes == allocatedRes_)
            return;

        // Descriptors and per-mip views point at the image about to be replaced, and PreRender
        // runs inside the frame's already-recording command buffer; same blunt idle SSR uses.
        if (VkResult r = vkDeviceWaitIdle(device_); r < 0)
        {
            Log::Critical("[HiZ] vkDeviceWaitIdle in EnsureSized failed: {}", RenderSystem::StrVkResult(r));
            RenderSystem::CheckVkResult(r);
        }

        DestroyMipViews();
        CreatePyramid(pyramidRes);
        CreateMipViews();
        BuildDescriptors();
        ++generation_;
    }

    void HiZPipelinePass::PreRender(const CommandBuffer &cmd)
    {
        auto *depthImg = dynamic_cast<const ImageDepth *>(RenderSystem::Get()->GetAttachment("gbuf_depth"));
        if (!depthImg)
            return;

        EnsureSized(depthImg->GetSize());
        if (!pyramid_ || levelCount_ == 0)
            return;

        // gbuf_depth's tracked layout after stage 0's renderpass ends is
        // DEPTH_STENCIL_READ_ONLY_OPTIMAL (same assumption SSRPipelinePass makes).
        if (depthImg->GetView() != lastDepthView_)
        {
            DescriptorSetWriteBuilder(*levelSets_[0])
                    .Image(0, depthImg->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                           VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    .Build()
                    .Apply();
            lastDepthView_ = depthImg->GetView();
        }

        auto barrier = [&](VkAccessFlags src, VkAccessFlags dst)
        {
            Image::InsertImageMemoryBarrier(cmd, pyramid_->GetImage(), src, dst, VK_IMAGE_LAYOUT_GENERAL,
                                            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                                            levelCount_, 0, 1, 0);
        };

        // Last frame's SSR Trace reads must finish before this frame overwrites the pyramid.
        barrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        downsamplePipeline_->BindPipeline(cmd);
        for (uint32_t k = 0; k < levelCount_; ++k)
        {
            levelSets_[k]->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, downsamplePipeline_->GetPipelineLayout(),
                                                 VK_PIPELINE_BIND_POINT_COMPUTE);

            const UVec3 levelRes{std::max(1u, allocatedRes_.x >> k), std::max(1u, allocatedRes_.y >> k), 1u};
            downsamplePipeline_->Dispatch(cmd, levelRes, {8, 8, 1});

            // Level k+1 reads level k; the final barrier also publishes the whole pyramid to SSR.
            barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
    }
} // namespace SF::Engine
