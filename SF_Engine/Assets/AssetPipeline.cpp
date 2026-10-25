#include "AssetPipeline.hpp"

#include <Assets/Audio/AudioAsset.hpp>
#include <Assets/Models/MeshAsset.hpp>
#include <Engine/Log/Log.hpp>
#include <Engine/Project/Project.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <Rendering/RHI/Images/Image.hpp>

namespace SF::Engine
{
    std::unordered_map<std::string, AssetFactoryFn> &AssetController::Factories()
    {
        static std::unordered_map<std::string, AssetFactoryFn> factories;
        return factories;
    }

    void AssetController::RegisterFactory(const std::string &rttiTypeName, AssetFactoryFn factory)
    {
        auto &factories = Factories();
        if (factories.contains(rttiTypeName))
        {
            Log::Error("AssetController: factory for '{}' registered more than once; ignoring duplicate.",
                       rttiTypeName);
            return;
        }
        factories.emplace(rttiTypeName, std::move(factory));
    }

    void AssetController::RegisterBuiltinFactories()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        // Temporaries: constructing an AssetRegistrar is what registers the factory.
        (void) AssetRegistrar<MeshAsset>{};
        (void) AssetRegistrar<AudioAsset>{};
        (void) AssetRegistrar<ImageAsset<Image2d>>{};
    }

    bool AssetController::Initialize()
    {
        RegisterBuiltinFactories();
        assets_ = vector<shared_ptr<AssetBase>>();
        return true;
    }

    void AssetController::ProjectLoaded()
    {
        // Drop assets that came from a previous project's disk scan so reloading doesn't duplicate them.
        // Procedural assets (no assetPath, e.g. engine LUTs registered at startup) are kept.
        std::erase_if(assets_, [](const std::shared_ptr<AssetBase> &a) { return a && !a->assetPath.empty(); });

        const std::filesystem::path assetsRoot = ProjectManager::Get()->GetProjectAssetPath();
        if (!std::filesystem::exists(assetsRoot))
        {
            Log::Warning("AssetController: assets folder '{}' does not exist yet.", assetsRoot.string());
            return;
        }

        const auto &factories = Factories();
        std::error_code ec;

        for (auto it = std::filesystem::recursive_directory_iterator(assetsRoot, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                Log::Error("AssetController: error scanning '{}': {}", assetsRoot.string(), ec.message());
                break;
            }
            if (it->is_directory() || it->path().extension() != ".meta")
                continue;

            const std::filesystem::path &metaPath = it->path();

            XMLModule *reader = XMLModule::Get();
            if (!reader->LoadFromFile(metaPath.string()))
            {
                Log::Error("AssetController: failed to parse meta '{}': {}", metaPath.string(), reader->GetLastError());
                continue;
            }

            XMLNode metaRoot  = reader->GetRootNode();
            XMLNode assetNode = metaRoot.GetChild("Asset");
            if (!assetNode.IsValid())
            {
                Log::Error("AssetController: meta '{}' has no <Asset> node; skipping.", metaPath.string());
                continue;
            }

            int rawType{};
            assetNode.GetAttribute("Type", rawType);
            // const AssetType assetType = static_cast<AssetType>(rawType);

            std::string concreteType;
            assetNode.GetAttribute("ConcreteType", concreteType);

            auto factoryIt = factories.find(concreteType);
            if (factoryIt == factories.end())
            {
                std::string name;
                assetNode.GetAttribute("Name", name);
                Log::Error("AssetController: no factory for type '{}' (asset '{}'); skipping.", concreteType, name);
                continue;
            }

            std::shared_ptr<AssetBase> asset = factoryIt->second();

            // "Foo.shader.meta" -> "Foo.shader" (only the trailing .meta is stripped)
            std::filesystem::path assetDataPath = metaPath;
            assetDataPath.replace_extension();

            asset->assetPath = assetDataPath;
            asset->Deserialize(metaRoot);

            assets_.push_back(std::move(asset));
        }

        Log::Info("AssetController: loaded {} assets from '{}'.", assets_.size(), assetsRoot.string());
    }

    void AssetController::SaveAll()
    {
        for (auto &asset: assets_)
        {
            if (!asset)
                continue;
            asset->Save();     // write the payload/data file
            asset->SaveMeta(); // write just this asset's .meta sidecar
        }
    }

    std::shared_ptr<AssetBase> AssetController::FindByUUID(const UUID &guid) const
    {
        for (const auto &asset: assets_)
        {
            if (asset && asset->uuid == guid)
            {
                return asset;
            }
        }

        return nullptr;
    }

    std::shared_ptr<AssetBase> AssetController::FindByName(std::string_view name) const
    {
        for (const auto &asset: assets_)
        {
            if (asset && asset->name == name)
            {
                return asset;
            }
        }

        return nullptr;
    }

    std::shared_ptr<AssetBase> AssetController::FindByPath(const std::filesystem::path &path) const
    {
        if (path.empty())
            return nullptr;

        std::error_code ec;
        const std::filesystem::path wanted = std::filesystem::weakly_canonical(path, ec);

        for (const auto &asset: assets_)
        {
            if (!asset || asset->assetPath.empty())
                continue;
            if (asset->assetPath == path)
                return asset;

            std::error_code ec2;
            if (!ec && std::filesystem::weakly_canonical(asset->assetPath, ec2) == wanted && !ec2)
                return asset;
        }
        return nullptr;
    }

    void AssetController::AddAsset(std::shared_ptr<AssetBase> asset)
    {
        if (asset)
            assets_.push_back(std::move(asset));
    }

    void AssetController::SaveManifest()
    {
        if (!ProjectManager::Get()->IsAProjectLoaded())
        {
            Log::Warning("AssetController: SaveManifest called with no project loaded; ignoring.");
            return;
        }

        const std::filesystem::path manifestPath = ProjectManager::Get()->GetProjectAssetPath() / "AssetManifest.xml";

        XMLModule *writer = XMLModule::Get();
        writer->SetRootNode("AssetManifest");
        XMLNode root = writer->GetRootNode();

        for (const auto &asset: assets_)
        {
            if (asset)
                asset->Serialize(root);
        }

        if (!writer->SaveToFile(manifestPath.string()))
        {
            Log::Error("AssetController: failed to save asset manifest to '{}'", manifestPath.string());
        }
    }
} // namespace SF::Engine
