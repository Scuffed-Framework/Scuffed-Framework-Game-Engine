#include "AssetPipeline.hpp"

#include <LowLevel/FileSystem/File.hpp>
#include <Engine/Log/Log.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <assimp/Importer.hpp>
#include <Engine/Project/Project.hpp>

namespace SF::Engine
{
    std::unordered_map<std::string, AssetFactoryFn> &AssetController::Factories()
    {
        static std::unordered_map<std::string, AssetFactoryFn> factories;
        return factories;
    }

    void AssetController::RegisterFactory(AssetType type, const std::string &rttiTypeName, AssetFactoryFn factory)
    {
        auto &factories = Factories();
        if (factories.contains(rttiTypeName))
        {
            Log::Error("AssetController: factory for '{}' registered more than once; ignoring duplicate.", rttiTypeName);
            return;
        }
        factories.emplace(rttiTypeName, std::move(factory));
        // type is kept for filtering only — you can still index it separately if you want fast "all Textures" queries
    }

    bool AssetController::Initialize()
    {
        // Actual asset loading happens per-project in ProjectLoaded(), once we
        // know which Assets/ folder to scan. Nothing to do at engine boot.
        assets_ = SFTL::DynamicArray<std::shared_ptr<AssetBase>>();
        return true;
    }

    void AssetController::ProjectLoaded()
    {
        if(assets_.size() == 0)
            assets_.clear();

        const std::filesystem::path assetsRoot = ProjectManager::Get()->GetProjectAssetPath();
        if (!std::filesystem::exists(assetsRoot))
        {
            Log::Warning("AssetController: assets folder '{}' does not exist yet.", assetsRoot.string());
            return;
        }

        const auto &factories = Factories();
        std::error_code ec;

        for (auto it = std::filesystem::recursive_directory_iterator(assetsRoot, ec);
             it != std::filesystem::recursive_directory_iterator();
             it.increment(ec))
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

            XMLNode metaRoot = reader->GetRootNode();
            XMLNode assetNode = metaRoot.GetChild("Asset");
            if (!assetNode.IsValid())
            {
                Log::Error("AssetController: meta '{}' has no <Asset> node; skipping.", metaPath.string());
                continue;
            }

            int rawType{};
            assetNode.GetAttribute("Type", rawType);
            const AssetType assetType = static_cast<AssetType>(rawType);

            std::string concreteType;
            assetNode.GetAttribute("ConcreteType", concreteType);

            auto factoryIt = factories.find(concreteType);
            if (factoryIt == factories.end())
            {
                std::string name;
                assetNode.GetAttribute("Name", name);
                Log::Error("AssetController: no factory for type '{}' (asset '{}'); skipping.",
                        concreteType, name);
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
        for (auto &asset : assets_)
        {
            if (!asset)
                continue;
            asset->Save();     // write the payload/data file
            asset->SaveMeta(); // write just this asset's .meta sidecar
        }
    }

    std::shared_ptr<AssetBase> AssetController::FindByUUID(const UUID &guid) const
    {
        for (const auto &asset : assets_)
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
        for (const auto &asset : assets_)
        {
            if (asset && asset->name == name)
            {
                return asset;
            }
        }

        return nullptr;
    }

    void AssetController::SaveManifest()
    {
        if (!ProjectManager::Get()->IsAProjectLoaded())
        {
            Log::Warning("AssetController: SaveManifest called with no project loaded; ignoring.");
            return;
        }

        const std::filesystem::path manifestPath =
            ProjectManager::Get()->GetProjectAssetPath() / "AssetManifest.xml";

        XMLModule *writer = XMLModule::Get();
        writer->SetRootNode("AssetManifest");
        XMLNode root = writer->GetRootNode();

        for (const auto &asset : assets_)
        {
            if (asset)
                asset->Serialize(root);
        }

        if (!writer->SaveToFile(manifestPath.string()))
        {
            Log::Error("AssetController: failed to save asset manifest to '{}'", manifestPath.string());
        }
    }
}