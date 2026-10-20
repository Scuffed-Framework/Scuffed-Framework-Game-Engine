#include "EngineRenderpassManager.hpp"

namespace SF::Engine
{
    void EngineRenderpassManager::Clear() { stages.clear(); }

    void EngineRenderpassManager::RemoveSFERenderpassStage(const TypeId &id)
    {
        for (auto it = stages.begin(); it != stages.end();)
        {
            if (it->second == id)
            {
                it = stages.erase(it);
            } else
            {
                ++it;
            }
        }
    }

    void EngineRenderpassManager::PreRenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer)
    {
        for (const auto &[stageIndex, typeId]: stages)
        {
            if (stageIndex.first != stage)
                continue;

            if (auto &SFERenderpass = SFERenderpasses[typeId])
                if (SFERenderpass->IsEnabled())
                    SFERenderpass->PreRender(commandBuffer);
        }
    }

    void EngineRenderpassManager::RenderStage(const Pipeline::Stage &stage, const CommandBuffer &commandBuffer)
    {
        for (const auto &[stageIndex, typeId]: stages)
        {
            if (stageIndex.first != stage)
            {
                continue;
            }

            if (auto &SFERenderpass = SFERenderpasses[typeId])
            {
                if (SFERenderpass->IsEnabled())
                {
                    SFERenderpass->Render(commandBuffer);
                }
            }
        }
    }
} // namespace SF::Engine
