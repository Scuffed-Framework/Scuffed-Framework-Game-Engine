#pragma once
#include <Assets/AssetPipeline.hpp>

#include "Cubemap.hpp"
#include "Image2d.hpp"
#include "Image2dArray.hpp"
#include "Image3d.hpp"
#include <Engine/Engine.hpp>
#include <fstream>

namespace SF::Engine
{
    template <typename TImage = Image2d>
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
            XMLModule *writer = XMLModule::Get();
            writer->SetRootNode(RTTI_TypeName());
            XMLNode root = writer->GetRootNode();
            AssetBase::Serialize(root);

            root.SetAttribute("Filename", filename.string());
            if (texture)
            {
                root.SetAttribute("Filter", static_cast<int>(texture->GetFilter()));
                root.SetAttribute("AddressMode", static_cast<int>(texture->GetAddressMode()));
                root.SetAttribute("Format", static_cast<int>(texture->GetFormat()));
                root.SetAttribute("Samples", static_cast<int>(texture->GetSamples()));
                root.SetAttribute("MipLevels", static_cast<int>(texture->GetMipLevels()));
                root.SetAttribute("ArrayLayers", static_cast<int>(texture->GetArrayLevels()));
                root.SetAttribute("UsageBits", static_cast<int>(texture->GetUsage()));
                root.SetAttribute("Layout", static_cast<int>(texture->GetLayout()));
            }

            writer->SaveToFile((GetEngineAssetsPath() / (name + ".xml")).string());
        }

        bool Load(std::span<const uint8_t> payload) override
        {
            if (filename.empty())
                return false;

            XMLModule *writer = XMLModule::Get();
            XMLNode root = writer->GetRootNode();
            AssetBase::Deserialize(root);

            auto bitmap = std::make_unique<Bitmap>(filename);
            if (!bitmap || !*bitmap)
                return false;

            int rawFormat{}, rawLayout{}, rawUsage{}, rawFilter{}, rawAddressMode{};
            root.GetAttribute("Format", rawFormat);
            root.GetAttribute("Layout", rawLayout);
            root.GetAttribute("UsageBits", rawUsage);
            root.GetAttribute("Filter", rawFilter);
            root.GetAttribute("AddressMode", rawAddressMode);

            auto format = static_cast<VkFormat>(rawFormat);
            auto layout = static_cast<VkImageLayout>(rawLayout);
            auto usage = static_cast<VkImageUsageFlags>(rawUsage);
            auto filter = static_cast<VkFilter>(rawFilter);
            auto addressMode = static_cast<VkSamplerAddressMode>(rawAddressMode);

            if constexpr (std::is_same_v<TImage, Image2d> || std::is_same_v<TImage, Cubemap>)
            {
                texture = std::make_shared<TImage>(
                    std::move(bitmap), format, layout, usage, filter, addressMode);
            }
            else if constexpr (std::is_same_v<TImage, Image2dArray>)
            {
                int rawLayerCount{1};
                root.GetAttribute("ArrayLayers", rawLayerCount);
                texture = std::make_shared<TImage>(
                    std::move(bitmap), static_cast<uint32_t>(rawLayerCount),
                    format, layout, usage, filter, addressMode);
            }
            else if constexpr (std::is_same_v<TImage, Image3d>)
            {
                // TODO: Saving 3d textures
                return false;
            }
            else
            {
                static_assert(!sizeof(TImage), "ImageAsset<TImage>::Load: no loading strategy for this TImage");
            }

            return texture != nullptr;
        }

        // Construct the live TImage directly, forwarding whatever ctor args
        // TImage actually needs (extent, format, filter, voxel data, etc).
        // This is what makes ImageAsset<TImage> work uniformly across
        // Image2d/Image3d/Image2dArray without per-type subclasses; each
        // one just gets called with its own natural constructor signature.
        template <typename... Args>
        void Create(Args &&...args)
        {
            texture = std::make_shared<TImage>(std::forward<Args>(args)...);
        }
    };
}