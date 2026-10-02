#include <Rendering/RHI/Images/Image.hpp>
#include "Color/Color.hpp"

namespace SF::Engine
{
    Image2d makeTextureFromColor(Color col)
    {
        auto bitmap = std::make_unique<Bitmap>(UVec2{1, 1}, 4);

        const uint32_t packed = col.ToInt(Color::PackingOrder::RGBA);
        uint8_t *data         = bitmap->GetData().get();

        // RGBA order, byte-for-byte
        data[0] = static_cast<uint8_t>((packed >> 24) & 0xFF); // R
        data[1] = static_cast<uint8_t>((packed >> 16) & 0xFF); // G
        data[2] = static_cast<uint8_t>((packed >> 8) & 0xFF);  // B
        data[3] = static_cast<uint8_t>(packed & 0xFF);         // A

        return Image2d(std::move(bitmap), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_FILTER_LINEAR,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, false, false);
    }

    struct PBRMaterialPushConstatns
    {
        Vec4 baseColor; // rgba
        float metallicFactor;
        float roughnessFactor;
        float aoFactor;
        float emissiveFactor;

        float heightScale;
        float tessellationFactor;
        float tessMinDistance;
        float tessMaxDistance;
    };
    static_assert(std::is_trivially_copyable_v<PBRMaterialPushConstatns>);

    struct PBRMaterial
    {
        // Scalars / colors
        Color baseColor = Color::White;
        // Defaults to dielectric, not metallic: an object with no material assigned should
        // render as a plain white diffuse surface (what "white, unconfigured" reads as to
        // anyone placing an object), not a mirror. At metallic=1, DeferredLight.shader's
        // diffuse term `(1 - F) * (1 - metal) * albedo` is forced to exactly zero regardless
        // of baseColor -- a fully metallic surface only ever shows reflected environment
        // color (via SSR's F0=albedo tinted specular/ProbeFallback), never its own flat
        // color. That's correct PBR behavior for an object that's actually supposed to be
        // metal, but it's a surprising default for one that just hasn't been configured yet.
        float metallicFactor     = 0.0f;
        float roughnessFactor    = 1.0f;
        float aoFactor           = 1.0f;
        float emissiveFactor     = 0.0f;
        float displacementFactor = 0.05f;
        float tessellationFactor = 16.0f;
        float tessMinDistance    = 5.0f;
        float tessMaxDistance    = 50.0f;

        // Textures (nullable = use defaults)
        std::shared_ptr<Image2d> albedo;
        std::shared_ptr<Image2d> normal;
        std::shared_ptr<Image2d> roughness;
        std::shared_ptr<Image2d> ao;
        std::shared_ptr<Image2d> displacement;
    };
}