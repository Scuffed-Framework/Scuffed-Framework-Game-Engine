#include "Image.hpp"
namespace SF::Engine
{
    // i hate this language
    static AssetRegistrar<ImageAsset<Image2d>> s_registerTexture2d();
    static AssetRegistrar<ImageAsset<Image3d>> s_registerTexture3d();
    static AssetRegistrar<ImageAsset<Image2dArray>> s_registerTexture2dArray();
    static AssetRegistrar<ImageAsset<Cubemap>> s_registerCubemap();
} // namespace SF::Engine
