#pragma once

#include <Engine/Log/Log.hpp>
#include <Engine/Module.hpp>
#include <LowLevel/Reflection/RTTI/RTTICast.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <UtilityClasses/UUID.hpp>
#include <filesystem>
#include <span>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    enum class AssetType // scoped enum avoids name collisions
    {
        Mesh,
        Texture,
        Shader,
        LuaScript, // Lua
        CppCode,   // we should be able to run C++ code like in unreal
        VFX,
        Audio,
        SkeletalAnimation,
        Scene,
        TemplateAsset,
        ConfigFile,
        Font,
        Material,
        Library, // Dll(...so, dylib...), or rsc file
        AnimationStateMachine
    };

    // The editor-side half of the Asset-Resource-Protocol (see Assets/AssetAndResourceProtocol).
    // Everything below is written to the "<source file>.meta" sidecar. No keys, no payload copies.
    class AssetBase : public Serializable
    {
        SF_RTTI(AssetBase, Serializable)
    public:
        static constexpr int kSchemaVersion = 1;

        // The source file this asset was imported from (it sits next to its .meta). Empty for procedural assets.
        filesystem::path assetPath;
        string name;
        AssetType type;
        UUID uuid = UUID::Generate(); // stable asset ID: scenes reference assets by this, never by path

        int schemaVersion   = kSchemaVersion;
        int importerVersion = 0;  // bumped when an importer changes what it produces; lets us detect stale imports
        string sourceHash;        // BLAKE2b-256 (hex) of the source bytes at import time
        string packageId;         // filled in by the RSC build step; empty in the editor
        string recordLocator;     // ditto
        vector<UUID> dependencies; // assets this one needs (e.g. a model's textures)

        virtual ~AssetBase() = default;

        virtual void Save()                            = 0;
        virtual bool Load(span<const uint8_t> payload) = 0;

        // Import settings live in <Asset><ImportSettings>. Override to persist per-type settings.
        virtual void SerializeImportSettings(XMLNode & /*node*/) const {}
        virtual void DeserializeImportSettings(const XMLNode & /*node*/) {}

        // Builds the runtime resource (GPU buffers, OpenAL buffer, ...) from `assetPath`.
        // Idempotent and cheap once loaded. Returns false if the source is missing or unreadable.
        virtual bool EnsureLoaded() { return true; }

        void Serialize(XMLNode &node) const override
        {
            XMLNode asset = node.AddChild("Asset");
            asset.SetAttribute("Name", name);
            asset.SetAttribute("Type", static_cast<int>(type));
            asset.SetAttribute(string("UUID"), uuid.ToString());
            asset.SetAttribute("ConcreteType", string(RTTI_GetTypeName()));
            asset.SetAttribute("Schema", schemaVersion);
            asset.SetAttribute("ImporterVersion", importerVersion);
            asset.SetAttribute("SourceHash", sourceHash);
            if (!packageId.empty())
                asset.SetAttribute("PackageId", packageId);
            if (!recordLocator.empty())
                asset.SetAttribute("RecordLocator", recordLocator);

            if (!dependencies.empty())
            {
                XMLNode deps = asset.AddChild("Dependencies");
                for (const UUID &dep: dependencies)
                {
                    XMLNode d = deps.AddChild("Dependency");
                    d.SetAttribute(string("UUID"), dep.ToString());
                }
            }

            XMLNode settings = asset.AddChild("ImportSettings");
            SerializeImportSettings(settings);
        }

        void Deserialize(const XMLNode &node) override
        {
            XMLNode asset = node.GetChild("Asset");
            asset.GetAttribute("Name", name);
            int rawType{};
            asset.GetAttribute("Type", rawType);
            type = static_cast<AssetType>(rawType);
            uuid = UUID::FromString(asset.GetAttribute(string(string("UUID"))));

            // Everything below is optional so meta files written before the ARP fields existed still load.
            asset.GetAttribute("Schema", schemaVersion);
            asset.GetAttribute("ImporterVersion", importerVersion);
            asset.GetAttribute("SourceHash", sourceHash);
            asset.GetAttribute("PackageId", packageId);
            asset.GetAttribute("RecordLocator", recordLocator);

            dependencies.clear();
            if (XMLNode deps = asset.GetChild("Dependencies"); deps.IsValid())
            {
                for (const XMLNode &d: deps.GetChildren("Dependency"))
                {
                    string raw;
                    if (!d.GetAttribute("UUID", raw))
                        continue;
                    try
                    {
                        dependencies.push_back(UUID::FromString(raw));
                    } catch (const std::exception &)
                    {
                        Log::Warning("AssetBase: '{}' has a malformed dependency id '{}'; ignored.", name, raw);
                    }
                }
            }

            if (XMLNode settings = asset.GetChild("ImportSettings"); settings.IsValid())
                DeserializeImportSettings(settings);
        }

        void SaveMeta() const
        {
            if (assetPath.empty())
            {
                Log::Error("AssetBase::SaveMeta: '{}' has no assetPath; cannot write .meta", name);
                return;
            }
            XMLModule *writer = XMLModule::Get();
            writer->SetRootNode("AssetMeta");
            XMLNode root = writer->GetRootNode();
            Serialize(root);

            filesystem::path metaPath = assetPath;
            metaPath += ".meta";
            if (!writer->SaveToFile(metaPath.string()))
                Log::Error("AssetBase::SaveMeta: failed to write '{}'", metaPath.string());
        }
    };

    // Typed leaf that only adds the actual payload
    template<typename T>
    class Asset : public AssetBase
    {
        SF_RTTI(Asset<T>, AssetBase)
    public:
        T assetData;

        void Save() override
        {
            // write XML data changes
            // and implement in e.g. MeshAsset : Asset<Mesh>
        }
    };

    using AssetFactoryFn = function<shared_ptr<AssetBase>()>;

    class AssetController : public ModuleRegistrar<AssetController>
    {
        friend class ModuleRegistrar<AssetController>;
        REGISTER_MODULE(AssetController, Module::Stage::Normal);

    public:
        void Update() override {}
        bool Initialize() override;
        void ProjectLoaded();
        void Shutdown() override { assets_.clear(); }

        static void RegisterFactory(const string &rttiTypeName, AssetFactoryFn factory);

        // Registers MeshAsset / AudioAsset / ImageAsset<...> explicitly. Static-registrar objects in
        // otherwise-unreferenced .cpp files get dropped when SF_Engine is linked as a static library,
        // so the importable types are not left to static initialisation.
        static void RegisterBuiltinFactories();

        void SaveAll();
        void SaveManifest();

        [[nodiscard]] shared_ptr<AssetBase> FindByUUID(const UUID &guid) const;
        [[nodiscard]] shared_ptr<AssetBase> FindByName(string_view name) const;
        // Matches on the source file (assetPath), tolerant of different spellings of the same path.
        [[nodiscard]] shared_ptr<AssetBase> FindByPath(const filesystem::path &path) const;

        // Adds an already-constructed asset (e.g. freshly imported) to the live asset list.
        void AddAsset(shared_ptr<AssetBase> asset);

        template<typename T>
        shared_ptr<Asset<T>> GetAsset(const UUID &guid) const
        {
            return ::SF::RTTI::rtti_pointer_cast<Asset<T>>(FindByUUID(guid));
        }

        vector<shared_ptr<AssetBase>> assets_;

        template<typename T, typename... Args>
        shared_ptr<T> RegisterAsset(string assetName, Args &&...args)
        {
            static_assert(is_base_of_v<AssetBase, T>, "RegisterAsset<T> requires T to derive from AssetBase");

            auto asset  = make_shared<T>(forward<Args>(args)...);
            asset->name = std::move(assetName);
            assets_.push_back(asset);
            return asset;
        }

    private:
        static unordered_map<string, AssetFactoryFn> &Factories();
    };

    template<typename T>
    struct AssetRegistrar
    {
        static_assert(is_base_of_v<AssetBase, T>, "AssetRegistrar<T> requires T to derive from AssetBase "
                                                  "(this includes Asset<Payload> and ImageAsset<TImage> leaves)");

        explicit AssetRegistrar()
        {
            AssetController::RegisterFactory(T::RTTI_TypeName(), [] { return make_shared<T>(); });
        }
    };
} // namespace SF::Engine
