#pragma once

#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Buffers/UniformBuffer.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image2d.hpp>
#include <Rendering/RHI/Pipelines/ComputePipeline.hpp>
#include <Rendering/RHI/Pipelines/RhiRenderPipeline.hpp>
#include "LUT/MultiScatterLUT.hpp"
#include "LUT/SkyViewLUT.hpp"
#include "LUT/TransmittanceLUT.hpp"

#include <Math/BasicMath.hpp>
#include <memory>
#include "../../Water/OceanPipelinePass.hpp"

#include "LUT/AtmoLUTs.hpp"

namespace SF::Engine
{
    // Sky/aerial-perspective compute kernel writes into its own private
    // atmoColorRT_ image (never touches "hdr" directly). AtmosphereComposite
    // .shader then blends that into "hdr" as a real fullscreen-triangle
    // subpass draw in Render() - same PreRender-computes/Render-draws split
    // SSRPipelinePass uses, and for the identical reason: every attachment
    // in this engine's renderpasses uses loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
    // and PreRender for a whole render stage runs before that stage's
    // renderpass (and its clear) even begins. Writing "hdr" directly from
    // PreRender, as this pass used to, got unconditionally wiped by that
    // clear before DeferredLight's subpass - let alone this pass's own
    // subpass - ever ran.
    class AtmospherePipelinePass : public EngineRenderpass
    {
    public:
        explicit AtmospherePipelinePass(Pipeline::Stage stage, const AtmosphereParams &params = {});
        ~AtmospherePipelinePass() override = default;

        // Bakes LUTs and dispatches atmo_cs into atmoColorRT_. Does not
        // touch "hdr".
        void PreRender(const CommandBuffer &commandBuffer) override;

        // Fullscreen-triangle draw blending atmoColorRT_ into "hdr", as this
        // pass's actual subpass (see SceneRenderer.hpp for where it sits;
        // must be before DeferredLight's subpass draw returns non-background
        // pixels' final colour, i.e. this needs to run so its PremultipliedAlpha
        // background write doesn't clobber already-lit geometry - see .cpp).
        void Render(const CommandBuffer &commandBuffer) override;

        void SetSceneBuffers();
        void SetFrameData(const Mat4 &invProj, const Mat4 &invView, const Vec3 &cameraPos, const Vec3 &planetPos,
                          const Vec3 &sunDir, Vec2 screenSize);

        void SetParams(const AtmosphereParams &params) { params_ = params; }

        AtmosphereParams &GetParams() { return params_; }
        [[nodiscard]] const AtmosphereParams &GetParams() const { return params_; }

    private:
        void CreateComposite(Pipeline::Stage stage);

        AtmosphereParams params_;

        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;

        // Private compute output; sized to the real framebuffer resolution
        // (GetScreenSize(), now backed by Window::GetFramebufferSize() - see
        // SharedFunctions.cpp) the same way SSR sizes its own scratch images.
        std::unique_ptr<Image2d> atmoColorRT_;

        // Graphics : blends atmoColorRT_ into "hdr" as a real subpass draw.
        std::unique_ptr<RhiRenderPipeline> compositePipeline_;
        std::unique_ptr<DescriptorSet> compositeSet_;

        AtmosphereFrameUBO frameData_{};

        const Image2d *lastColor_    = nullptr;
        const ImageDepth *lastDepth_ = nullptr;
    };
} // namespace SF::Engine
