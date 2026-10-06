#include "Image.hpp"
namespace SF::Engine
{
    // i hate this language
    static AssetRegistrar<ImageAsset<Image2d>> s_registerTexture2d(AssetType::Texture);
    static AssetRegistrar<ImageAsset<Image3d>> s_registerTexture3d(AssetType::Texture);
    static AssetRegistrar<ImageAsset<Image2dArray>> s_registerTexture2dArray(AssetType::Texture);
    static AssetRegistrar<ImageAsset<Cubemap>> s_registerCubemap(AssetType::Texture);
} // namespace SF::Engine
