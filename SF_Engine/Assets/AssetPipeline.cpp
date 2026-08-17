#include "AssetPipeline.hpp"

#include <Filesystem/File.hpp>
#include <Engine/Log/Log.hpp>
#include <XML/XMLModule.hpp>
#include <assimp/Importer.hpp>
#include <Project/Project.hpp>

namespace SF::Engine
{
    std::unordered_map<AssetType, AssetFactoryFn> &AssetController::Factories()
    {
        static std::unordered_map<AssetType, AssetFactoryFn> factories;
        return factories;
    }

    void AssetController::RegisterFactory(AssetType type, AssetFactoryFn factory)
    {
        auto &factories = Factories();

        if (factories.contains(type))
        {
            Log::Error("AssetController: factory for AssetType {} registered more than once; ignoring duplicate.", static_cast<int>(type));
            return;
        }

        factories.emplace(type, std::move(factory));
    }

    void AssetController::ProjectLoaded()
    {
        const std::filesystem::path kManifestPath = ProjectManager::Get()->GetProjectAssetPath() / "AssetManifest.xml";

        if (!File::Exists(kManifestPath))
        {
            Log::Warning("AssetController: no manifest found at '{}', starting with an empty asset set.",
                         kManifestPath.string().c_str()); // Use .string().c_str()
        }

        XMLModule *reader = XMLModule::Get();
        if (!reader->LoadFromFile(kManifestPath.string()))
        {
            Log::Error("AssetController: failed to parse manifest '{}': {}",
                       kManifestPath.string().c_str(), // Use .string().c_str()
                       reader->GetLastError());
        }
    }

    bool AssetController::Initialize()
    {
        assets_.clear();

        XMLModule *reader = XMLModule::Get();

        XMLNode root = reader->GetRootNode();
        const auto &factories = Factories();

        for (XMLNode entry : root.GetChildren("Asset"))
        {
            int rawType{};
            entry.GetAttribute("Type", rawType);
            const AssetType assetType = static_cast<AssetType>(rawType);

            auto factoryIt = factories.find(assetType);
            if (factoryIt == factories.end())
            {
                std::string name;
                entry.GetAttribute("Name", name);
                Log::Error("AssetController: no factory registered for AssetType {} (asset '{}'); skipping.",
                           static_cast<int>(assetType), name);
                continue;
            }

            std::shared_ptr<AssetBase> asset = factoryIt->second();

            entry.GetAttribute("Name", asset->name);
            asset->type = assetType;
            asset->uuid = UUID::FromString(entry.GetAttribute(std::string(std::string("UUID"))));

            assets_.push_back(std::move(asset));
        }

        return true;
        Log::Info("AssetController: loaded {} asset entries from manifest.", assets_.size());
    }

    void AssetController::SaveAll()
    {
        for (auto &asset : assets_)
        {
            if (!asset)
            {
                continue;
            }

            asset->Save();
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
}