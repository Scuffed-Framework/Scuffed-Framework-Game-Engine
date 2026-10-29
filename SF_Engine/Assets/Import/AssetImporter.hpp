#pragma once

#include <Assets/AssetPipeline.hpp>
#include <assimp/Importer.hpp>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace SF::Engine
{
    enum class ImportKind
    {
        Unknown,
        Model,
        Audio,
        Image
    };

    struct ImportResult
    {
        std::filesystem::path source;
        std::shared_ptr<AssetBase> asset;
        bool alreadyImported = false; // the file was already an asset; `asset` is the existing one
        std::string error;            // empty on success

        [[nodiscard]] bool Ok() const { return asset != nullptr && error.empty(); }
    };

    // Editor-time importer: file on disk -> file inside <project>/Assets + "<file>.meta" + live asset.
    // Cooking to .tbf/.rsc is the build pipeline's job (see AssetAndResourceProtocol), not this.
    class AssetImporter
    {
    public:
        // todo: multi instance for multithreading
        static std::unique_ptr<Assimp::Importer> importer;
        static constexpr int kImporterVersion = 1;

        [[nodiscard]] static ImportKind Classify(const std::filesystem::path &file);
        [[nodiscard]] static bool IsSupported(const std::filesystem::path &file)
        {
            return Classify(file) != ImportKind::Unknown;
        }

        AssetImporter() { importer = std::make_unique<Assimp::Importer>(); }

        // Imports one file. Files outside the project's Assets folder are copied into `destDir`
        // (clamped to Assets/); files already inside it are imported in place.
        // Must be called on the main thread (creates GPU / OpenAL resources).
        static ImportResult Import(const std::filesystem::path &source, const std::filesystem::path &destDir);

        static std::vector<ImportResult> ImportMany(std::span<const std::filesystem::path> sources,
                                                    const std::filesystem::path &destDir);

        // BLAKE2b-256 of the file contents, lowercase hex. Empty on failure.
        [[nodiscard]] static std::string HashFile(const std::filesystem::path &file);
    };
} // namespace SF::Engine
