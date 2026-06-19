#include "SceneRenderer.hpp"
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    Image2d *SceneRenderer::GetHdrColorTarget()
    {
        auto *rs = RenderSystem::Get();
        auto *hdr = dynamic_cast<const Image2d *>(rs->GetAttachment("swapchain"));
        return const_cast<Image2d *>(hdr);
    }
}