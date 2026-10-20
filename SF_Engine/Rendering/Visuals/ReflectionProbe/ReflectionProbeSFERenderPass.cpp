#include "ReflectionProbeSFERenderPass.hpp"

#include <Rendering/RHI/Descriptors/DescriptorSetBuilder.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/SharedSamplers.hpp>

#include <algorithm>

namespace SF::Engine
{
    namespace
    {
        // Mirrors ProbeParams in Shaders/ReflectionProbe/ProbeSky.shader (std140: two float4 rows, then
        // four scalar ints -- deliberately NOT an int3, which would be 16-byte aligned and grow the struct).
        struct ProbeParamsGpu
        {
            Vec4 skyColor;    // rgb = analytic sky colour, a = intensity
            Vec4 groundColor; // rgb = analytic ground colour
            int32_t useAtmosphere;
            int32_t pad0;
            int32_t pad1;
            int32_t pad2;
        };
        static_assert(sizeof(ProbeParamsGpu) == 48, "ProbeParamsGpu must match ProbeParams in ProbeSky.shader");
    } // namespace

    ReflectionProbeSFERenderPass::ReflectionProbeSFERenderPass(Pipeline::Stage stage) : EngineRenderpass(stage)
    {
        // Same stage as the atmosphere ({1,0}); a large order so it runs after the atmosphere's PreRender
        // (which bakes the SkyView LUT this reads) no matter what order the frame graph assigns that pass,
        // and before SSR (a later subpass, {1,1}).
        EngineRenderpass::SetOrder(100);

        device_ = *RenderSystem::Get()->GetLogicalDevice();

        skyPipeline_       = std::make_unique<ComputePipeline>("Shaders/ReflectionProbe/ProbeSky.shader");
        prefilterPipeline_ = std::make_unique<ComputePipeline>("Shaders/ReflectionProbe/ProbePrefilter.shader");

        skySet_ = std::make_unique<DescriptorSet>(*skyPipeline_);
        for (uint32_t m = 1; m < kMipCount; ++m)
            prefilterSets_.push_back(std::make_unique<DescriptorSet>(*prefilterPipeline_));

        atmoUbo_   = std::make_unique<UniformBuffer>(sizeof(AtmosphereFrameUBO));
        paramsUbo_ = std::make_unique<UniformBuffer>(sizeof(ProbeParamsGpu));

        CreateCube();
    }

    ReflectionProbeSFERenderPass::~ReflectionProbeSFERenderPass()
    {
        // Views must die before cube_ (member destruction runs after this body).
        DestroyViews();
    }

    void ReflectionProbeSFERenderPass::DestroyViews()
    {
        if (device_ == VK_NULL_HANDLE)
            return;
        for (VkImageView v: mipViews_)
        {
            if (v != VK_NULL_HANDLE)
                vkDestroyImageView(device_, v, nullptr);
        }
        mipViews_.clear();
        if (srcView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_, srcView_, nullptr);
            srcView_ = VK_NULL_HANDLE;
        }
    }

    void ReflectionProbeSFERenderPass::CreateCube()
    {
        // mipmap=true allocates the full chain and (via CreateMipmaps) leaves every mip in `layout`;
        // the blit-generated contents are irrelevant, the first PreRender overwrites mips 0..kMipCount-1.
        cube_ = std::make_unique<Cubemap>(UVec2{kFaceSize, kFaceSize}, VK_FORMAT_R16G16B16A16_SFLOAT,
                                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, VK_FILTER_LINEAR,
                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, false, true);
        cube_->SetLayout(VK_IMAGE_LAYOUT_GENERAL);

        levelCount_ = std::min(cube_->GetMipLevels(), kMipCount);

        // Mip 0 as a cube the prefilter can sample, without including the mips it is writing.
        Image::CreateImageView(cube_->GetImage(), srcView_, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R16G16B16A16_SFLOAT,
                               VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 6, 0);

        mipViews_.assign(levelCount_, VK_NULL_HANDLE);
        for (uint32_t m = 0; m < levelCount_; ++m)
        {
            Image::CreateImageView(cube_->GetImage(), mipViews_[m], VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                                   VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, m, 6, 0);
        }

        // Sky pass: atmo UBO, params UBO, SkyView LUT, mip-0 storage view.
        {
            auto skyView = AtmoLUTs::Get().GetSkyViewLUT()->GetTexture(); // shared_ptr or raw: use -> either way
            DescriptorSetWriteBuilder(*skySet_)
                    .Buffer(0, atmoUbo_->GetBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .Buffer(1, paramsUbo_->GetBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .CombinedImageSampler(2, skyView->GetView(), skyView->GetSampler(),
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .Image(3, mipViews_[0], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .Build()
                    .Apply();
        }

        // Prefilter passes: sample mip 0, write mip m.
        for (uint32_t m = 1; m < levelCount_; ++m)
        {
            DescriptorSetWriteBuilder(*prefilterSets_[m - 1])
                    .Image(0, srcView_, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    .Image(1, mipViews_[m], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .Build()
                    .Apply();
        }

        captured_ = false;
        ++generation_;
    }

    void ReflectionProbeSFERenderPass::PreRender(const CommandBuffer &cmd)
    {
        if (!cube_ || levelCount_ < 2)
            return;

        ++frameCounter_;
        if (captured_ && (frameCounter_ % updateInterval_) != 0)
            return;

        ProbeParamsGpu params{};
        params.skyColor      = Vec4(analyticSky_, analyticIntensity_);
        params.groundColor   = Vec4(analyticGround_, 0.0f);
        params.useAtmosphere = atmosphere_ ? 1 : 0;
        paramsUbo_->Update(params);

        AtmosphereFrameUBO atmo{};
        if (atmosphere_)
            atmo = atmosphere_->GetFrameData();
        atmoUbo_->Update(atmo);

        auto barrier = [&](VkAccessFlags src, VkAccessFlags dst, uint32_t baseMip, uint32_t mipCount)
        {
            Image::InsertImageMemoryBarrier(cmd, cube_->GetImage(), src, dst, VK_IMAGE_LAYOUT_GENERAL,
                                            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT, mipCount,
                                            baseMip, 6, 0);
        };

        // Last frame's SSR Trace reads must finish before this frame overwrites the cube.
        barrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, 0, levelCount_);

        // 1) Sky -> mip 0.
        skyPipeline_->BindPipeline(cmd);
        skySet_->BindDescriptor(cmd);
        skyPipeline_->Dispatch(cmd, UVec3{kFaceSize, kFaceSize, 6}, {8, 8, 1});

        // Mip 0 must be visible to the prefilter's sampled reads.
        barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0, 1);

        // 2) GGX prefilter: one dispatch per mip, each reading mip 0 only (so no dependency between them).
        prefilterPipeline_->BindPipeline(cmd);
        for (uint32_t m = 1; m < levelCount_; ++m)
        {
            prefilterSets_[m - 1]->BindDescriptor(cmd);
            SharedSamplers::BindSharedSamplerSet(cmd, prefilterPipeline_->GetPipelineLayout(),
                                                 VK_PIPELINE_BIND_POINT_COMPUTE);

            const uint32_t size = std::max(1u, kFaceSize >> m);
            prefilterPipeline_->Dispatch(cmd, UVec3{size, size, 6}, {8, 8, 1});
        }

        // Publish the whole chain to SSR's Trace (compute, later this frame).
        barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0, levelCount_);

        captured_ = true;
    }
} // namespace SF::Engine
