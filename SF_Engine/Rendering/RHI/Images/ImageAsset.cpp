#include "Image.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
namespace SF::Engine
{
    // i hate this language
    static AssetRegistrar<ImageAsset<>> s_registerTexture2d();
    static AssetRegistrar<ImageAsset<Image3d>> s_registerTexture3d();
    static AssetRegistrar<ImageAsset<Image2dArray>> s_registerTexture2dArray();
    static AssetRegistrar<ImageAsset<Cubemap>> s_registerCubemap();
} // namespace SF::Engine
#pragma GCC diagnostic pop
