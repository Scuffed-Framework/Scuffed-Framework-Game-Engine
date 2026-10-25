#include "AssetImporter.hpp"

#include <Assets/Audio/AudioAsset.hpp>
#include <Assets/Bitmaps/Bitmap.hpp>
#include <Assets/Models/MeshAsset.hpp>
#include <Engine/Log/Log.hpp>
#include <Engine/Project/Project.hpp>
#include <Rendering/RHI/Images/Image.hpp>

#include <assimp/Importer.hpp>
#include <sodium.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace SF::Engine
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string Lower(std::string s)
        {
            std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        // True if `path` is `folder` or lives below it (component-wise).
        bool IsInside(const fs::path &path, const fs::path &folder)
        {
            std::error_code ec1, ec2;
            const fs::path p = fs::weakly_canonical(path, ec1);
            const fs::path f = fs::weakly_canonical(folder, ec2);
            if (ec1 || ec2)
                return false;
            const fs::path rel = p.lexically_relative(f);
            return !rel.empty() && *rel.begin() != fs::path("..");
        }

        fs::path UniquePath(const fs::path &desired)
        {
            std::error_code ec;
            const auto taken = [&](const fs::path &p)
            {
                fs::path meta = p;
                meta += ".meta";
                return fs::exists(p, ec) || fs::exists(meta, ec);
            };
            if (!taken(desired))
                return desired;

            const fs::path dir = desired.parent_path();
            const std::string stem = desired.stem().string();
            const std::string ext  = desired.extension().string();
            for (int i = 1; i < 10000; ++i)
            {
                fs::path candidate = dir / (stem + " (" + std::to_string(i) + ")" + ext);
                if (!taken(candidate))
                    return candidate;
            }
            return desired; // practically unreachable
        }

        const std::unordered_set<std::string> &AudioExtensions()
        {
            static const std::unordered_set<std::string> exts = {".wav", ".mp3", ".ogg", ".flac", ".opus"};
            return exts;
        }

        ImportResult Fail(ImportResult r, std::string message)
        {
            r.error = std::move(message);
            Log::Error("Import '{}': {}", r.source.string(), r.error);
            return r;
        }
    } // namespace

    ImportKind AssetImporter::Classify(const std::filesystem::path &file)
    {
        const std::string ext = Lower(file.extension().string());
        if (ext.empty())
            return ImportKind::Unknown;

        if (AudioExtensions().contains(ext))
            return ImportKind::Audio;

        // "Can the engine decode it" = "is a Bitmap loader registered for it".
        if (Bitmap::Registry().contains(Bitmap::NormalizeExtension(ext)))
            return ImportKind::Image;

        if (Assimp::Importer::IsExtensionSupported(ext))
            return ImportKind::Model;

        return ImportKind::Unknown;
    }

    std::string AssetImporter::HashFile(const std::filesystem::path &file)
    {
        if (sodium_init() < 0)
            return {};

        std::ifstream in(file, std::ios::binary);
        if (!in)
            return {};

        crypto_generichash_state state;
        crypto_generichash_init(&state, nullptr, 0, crypto_generichash_BYTES);

        std::vector<char> buffer(1u << 20);
        while (in.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || in.gcount() > 0)
        {
            crypto_generichash_update(&state, reinterpret_cast<const unsigned char *>(buffer.data()),
                                      static_cast<unsigned long long>(in.gcount()));
        }

        unsigned char digest[crypto_generichash_BYTES];
        crypto_generichash_final(&state, digest, sizeof(digest));

        char hex[crypto_generichash_BYTES * 2 + 1];
        sodium_bin2hex(hex, sizeof(hex), digest, sizeof(digest));
        return hex;
    }

    ImportResult AssetImporter::Import(const std::filesystem::path &source, const std::filesystem::path &destDir)
    {
        ImportResult result;
        result.source = source;

        auto *projects = ProjectManager::Get();
        auto *assets   = AssetController::Get();
        if (!projects || !projects->IsAProjectLoaded() || !assets)
            return Fail(std::move(result), "no project is loaded");

        std::error_code ec;
        if (!fs::is_regular_file(source, ec))
            return Fail(std::move(result), "not a regular file");

        const ImportKind kind = Classify(source);
        if (kind == ImportKind::Unknown)
            return Fail(std::move(result), "unsupported file type '" + source.extension().string() + "'");

        const fs::path assetsRoot = projects->GetProjectAssetPath();
        fs::path folder           = destDir.empty() ? assetsRoot : destDir;
        if (!IsInside(folder, assetsRoot))
            folder = assetsRoot;
        fs::create_directories(folder, ec);

        fs::path target;
        bool copied = false;
        if (IsInside(source, assetsRoot))
        {
            target = source; // already part of the project: import in place

            if (auto existing = assets->FindByPath(target))
            {
                result.asset           = std::move(existing);
                result.alreadyImported = true;
                return result;
            }
        } else
        {
            target = UniquePath(folder / source.filename());
            fs::copy_file(source, target, fs::copy_options::none, ec);
            if (ec)
                return Fail(std::move(result), "could not copy into project: " + ec.message());
            copied = true;
        }

        const auto cleanup = [&]
        {
            if (!copied)
                return;
            std::error_code rm;
            fs::remove(target, rm);
        };

        std::shared_ptr<AssetBase> asset;
        switch (kind)
        {
            case ImportKind::Model:
                asset       = std::make_shared<MeshAsset>();
                asset->type = AssetType::Mesh;
                break;
            case ImportKind::Audio:
                asset       = std::make_shared<AudioAsset>();
                asset->type = AssetType::Audio;
                break;
            case ImportKind::Image:
                asset       = std::make_shared<ImageAsset<Image2d>>();
                asset->type = AssetType::Texture;
                break;
            default:
                break;
        }

        asset->name            = target.stem().string();
        asset->assetPath       = target;
        asset->importerVersion = kImporterVersion;
        asset->sourceHash      = HashFile(target);

        // Importing means "prove it loads": this builds the runtime resource once, and fills in stats.
        if (!asset->EnsureLoaded())
        {
            cleanup();
            return Fail(std::move(result), "the file could not be read as " +
                                                   std::string(kind == ImportKind::Model   ? "a model"
                                                               : kind == ImportKind::Audio ? "audio"
                                                                                           : "an image"));
        }

        asset->SaveMeta();
        fs::path meta = target;
        meta += ".meta";
        if (!fs::exists(meta, ec))
        {
            cleanup();
            return Fail(std::move(result), "could not write " + meta.filename().string());
        }

        assets->AddAsset(asset);
        result.asset = std::move(asset);
        Log::Info("Imported '{}' -> {}", source.filename().string(), target.string());
        return result;
    }

    std::vector<ImportResult> AssetImporter::ImportMany(std::span<const std::filesystem::path> sources,
                                                        const std::filesystem::path &destDir)
    {
        std::vector<ImportResult> results;
        results.reserve(sources.size());
        for (const auto &s: sources)
            results.push_back(Import(s, destDir));
        return results;
    }
} // namespace SF::Engine
