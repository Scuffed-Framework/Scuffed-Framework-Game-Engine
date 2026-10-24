#pragma once
#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <Rendering/RHI/Memory/UniformBuffer.hpp>
#include <Rendering/RHI/Pipelines/RhiRenderPipeline.hpp>
#include <memory>

#include <Rendering/FrameGraph/EngineRenderpassInitRegistry.hpp>
#include <Rendering/Visuals/Sky/TimeManager.hpp>

namespace SF::Engine
{
    class SkyboxSFERenderpass : public EngineRenderpass
    {
        inline static bool s_registered = []()
        {
            EngineRenderpassInitRegistry::Get().Register(
                    [](EngineRenderpassManager &mgr)
                    {
                        mgr.Add<SkyboxSFERenderpass>(Pipeline::Stage{0, 0},
                                                     std::make_unique<SkyboxSFERenderpass>(Pipeline::Stage{0, 0}));
                    });
            return true;
        }();

    public:
        explicit SkyboxSFERenderpass(Pipeline::Stage stage);

        ~SkyboxSFERenderpass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

    private:
        std::unique_ptr<Cubemap> cubemap_;

        std::unique_ptr<RhiRenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;
    };
}