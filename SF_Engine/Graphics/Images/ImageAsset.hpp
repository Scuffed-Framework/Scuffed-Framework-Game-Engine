#pragma once
#include <Assets/AssetPipeline.hpp>

#include "Cubemap.hpp"
#include "Image2d.hpp"
#include "Image2dArray.hpp"
#include "Image3d.hpp"

namespace SF::Engine
{
    template <typename TImage>
    class ImageAsset : public AssetBase
    {
        SF_RTTI(ImageAsset<TImage>, AssetBase)
        static_assert(std::is_base_of_v<Image, TImage>,
                      "ImageAsset<TImage> requires TImage to derive from Image");

    public:
        std::shared_ptr<TImage> texture;
        std::filesystem::path filename; // empty if not disk-backed (e.g. procedural LUTs)

        void Save() override
        {
            XMLReader writer;
            writer.SetRootNode(RTTI_TypeName());
            XMLNode root = writer.GetRootNode();
            AssetBase::Serialize(root);

            root.SetAttribute("Filename", filename.string());
            if (texture)
            {
                // Pulled straight off the live Image, not duplicated as
                // separate asset fields.
                root.SetAttribute("Filter", static_cast<int>(texture->GetFilter()));
                root.SetAttribute("AddressMode", static_cast<int>(texture->GetAddressMode()));
                root.SetAttribute("Format", static_cast<int>(texture->GetFormat()));
                root.SetAttribute("Samples", static_cast<int>(texture->GetSamples()));
                root.SetAttribute("MipLevels", static_cast<int>(texture->GetMipLevels()));
                root.SetAttribute("ArrayLayers", static_cast<int>(texture->GetArrayLevels()));
                root.SetAttribute("Layout", static_cast<int>(texture->GetLayout()));
            }

            writer.SaveToFile((GetEngineAssetsPath() / (name + ".xml")).string());
        }

        bool Load(std::span<const uint8_t> payload) override
        {
            auto bitmap = Bitmap::DecodeFromMemory(payload);
            if (!bitmap)
                return false;
            texture = std::make_shared<TImage>(std::move(bitmap)); // uses TImage's Bitmap ctor
            return texture != nullptr;
        }

        // Construct the live TImage directly, forwarding whatever ctor args
        // TImage actually needs (extent, format, filter, voxel data, etc).
        // This is what makes ImageAsset<TImage> work uniformly across
        // Image2d/Image3d/Image2dArray without per-type subclasses — each
        // one just gets called with its own natural constructor signature.
        template <typename... Args>
        void Create(Args &&...args)
        {
            texture = std::make_shared<TImage>(std::forward<Args>(args)...);
        }
    };

    /*
    void SomeInitFunction()
    {
        auto Image2dAsset = AssetController::Get().RegisterAsset<ImageAsset<Image2d>>("MyTexture");
        auto Image3dAsset = AssetController::Get().RegisterAsset<ImageAsset<Image3d>>("CloudNoise");
        auto Image2dArrayAsset = AssetController::Get().RegisterAsset<ImageAsset<Image2dArray>>("ShadowCascades");
    }
    */
}