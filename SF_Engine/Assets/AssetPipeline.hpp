#pragma once

#include <Engine/Log/Log.hpp>
#include <Engine/Module.hpp>
#include <LowLevel/Reflection/RTTI/RTTICast.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <UtilityClasses/UUID.hpp>
#include <filesystem>
#include <span>

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

    class AssetBase : public Serializable
    {
        SF_RTTI(AssetBase, Serializable)
    public:
        filesystem::path assetPath;
        string name;
        AssetType type;
        UUID uuid = UUID::Generate();


        virtual ~AssetBase() = default;

        virtual void Save()                            = 0;
        virtual bool Load(span<const uint8_t> payload) = 0;

        void Serialize(XMLNode &node) const override
        {
            XMLNode asset = node.AddChild("Asset");
            asset.SetAttribute("Name", name);
            asset.SetAttribute("Type", static_cast<int>(type));
            asset.SetAttribute(string("UUID"), uuid.ToString());
            asset.SetAttribute("ConcreteType", string(RTTI_GetTypeName()));
        }

        void Deserialize(const XMLNode &node) override
        {
            XMLNode asset = node.GetChild("Asset");
            asset.GetAttribute("Name", name);
            int rawType{};
            asset.GetAttribute("Type", rawType);
            type = static_cast<AssetType>(rawType);
            uuid = UUID::FromString(asset.GetAttribute(string(string("UUID"))));
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

        static void RegisterFactory(AssetType type, const string &rttiTypeName, AssetFactoryFn factory);

        void SaveAll();
        void SaveManifest();

        [[nodiscard]] shared_ptr<AssetBase> FindByUUID(const UUID &guid) const;
        [[nodiscard]] shared_ptr<AssetBase> FindByName(string_view name) const;

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
                                                  "(this includes Asset<Payload> and ImageAssetBase<TImage> leaves)");

        explicit AssetRegistrar(AssetType type)
        {
            AssetController::RegisterFactory(type, T::RTTI_TypeName(), [] { return make_shared<T>(); });
        }
    };
} // namespace SF::Engine
