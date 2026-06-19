#include "LightingRenderer.hpp"
#include <Graphics/RenderSystem.hpp>
namespace SF::Engine
{
    Image2d *LightingRenderer::GetHdrColorTarget()
    {
        auto *rs = RenderSystem::Get();
        auto *hdr = dynamic_cast<const Image2d *>(rs->GetAttachment("hdr"));
        return const_cast<Image2d *>(hdr);
    }
}