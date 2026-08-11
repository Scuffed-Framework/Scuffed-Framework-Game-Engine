#pragma once

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Pipelines/RenderPipeline.hpp>
#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Buffers/Buffer.hpp>
#include <memory>
#include <string>

namespace SF::Engine
{
    /**
     * @brief PipelinePass that performs a fullscreen blit/post-process pass.
     *
     * Uses a single fullscreen triangle (no VBO) to sample a source texture
     * and write it into the current render target (typically the swapchain).
     *
     * Usage:
     * Add a second render stage to your Renderer that has:
     *   - Attachment{0, "scene", Attachment::Type::Image}   <- the resolved scene colour
     *   - Attachment{1, "swapchain", Attachment::Type::Swapchain}
     * Then AddPipelinePass<FullscreenPass>({stageIndex, 0}, "scene");
     *
     * The source attachment name is resolved each frame via RenderSystem::GetAttachment().
     */
    class FullscreenPass : public PipelinePass
    {
    public:
        /**
         * @param stage          Pipeline stage (renderStageIndex, subpassIndex).
         * @param sourceAttachment  Name of the Image attachment to blit from.
         * @param shaderPath     Path to the .shader file (defaults to the built-in one).
         */
        explicit FullscreenPass(Pipeline::Stage stage,
                                std::string sourceAttachment = "scene",
                                const std::filesystem::path &shaderPath =
                                    "Shaders/FullscreenPass.shader");

        ~FullscreenPass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

    private:
        std::string sourceAttachment_;
        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descriptorSet_;

        // Tracks the last image pointer so we only rewrite descriptors on change.
        const Image2d *lastBoundImage_ = nullptr;
    };
}
