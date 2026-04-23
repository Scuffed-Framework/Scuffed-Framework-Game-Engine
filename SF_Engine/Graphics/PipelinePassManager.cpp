#include "PipelinePassManager.hpp"

namespace SF::Engine
{
    void PipelinePassManager::Clear()
    {
        stages.clear();
    }

    void PipelinePassManager::RemovePipelinePassStage(const TypeId &id)
    {
        for (auto it = stages.begin(); it != stages.end();)
        {
            if (it->second == id)
            {
                it = stages.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void PipelinePassManager::PreRenderStage(const Pipeline::Stage &stage,
                                             const CommandBuffer &commandBuffer)
    {
        for (const auto &[stageIndex, typeId] : stages)
        {
            if (stageIndex.first != stage)
                continue;

            if (auto &PipelinePass = PipelinePasss[typeId])
                if (PipelinePass->IsEnabled())
                    PipelinePass->PreRender(commandBuffer);
        }
    }

    void PipelinePassManager::RenderStage(const Pipeline::Stage &stage,
                                          const CommandBuffer &commandBuffer)
    {
        for (const auto &[stageIndex, typeId] : stages)
        {
            if (stageIndex.first != stage)
            {
                continue;
            }

            if (auto &PipelinePass = PipelinePasss[typeId])
            {
                if (PipelinePass->IsEnabled())
                {
                    PipelinePass->Render(commandBuffer);
                }
            }
        }
    }
}
