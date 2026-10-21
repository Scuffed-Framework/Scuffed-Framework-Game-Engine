#pragma once

#include "ResourceFileHeader.hpp"
#include "RscCrypto.hpp"
#include <LowLevel/FileSystem/File.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace SF::Engine
{
    //  RscPacker
    //
    //  Build/tool-side only, never links into the game runtime.
    //
    //  Pack pipeline per asset:
    //    raw bytes
    //      → RscByteShuffler::Shuffle()      (stride auto-selected, stored in entry)
    //      → Compress()                      (LZ4 / ZSTD / None)
    //      → RscCipher::Encrypt()            (ChaCha20-Poly1305, tag appended)
    //
    //  Archive signature written last:
    //    BLAKE2b-128( header[0..47] || entry_table ) → header.archiveSignature
    //
    //  Typical usage:
    //
    //    RscKeyDeriver::BuildInfo info { timestamp, engineVer, platformSalt };
    //
    //    RscPacker packer(info);
    //    packer.AddFile("textures/albedo.ktx2", RscAssetType::Texture, RscCompression::ZSTD);
    //    packer.AddFile("meshes/cube.mesh",     RscAssetType::Mesh,    RscCompression::LZ4);
    //    packer.Pack("assets/game.rsc");
    class RscPacker
    {
    public:
        using ProgressFn = std::function<void(std::string_view name, size_t current, size_t total)>;

        explicit RscPacker(const RscKeyDeriver::BuildInfo &buildInfo);

        // Read a loose file from disk and stage it under assetName
        bool AddFile(const std::string &assetName,
                     const std::string &sourcePath,
                     RscAssetType type = RscAssetType::Unknown,
                     RscCompression compression = RscCompression::LZ4,
                     uint8_t shuffleStride = RscByteShuffler::AUTO_STRIDE);

        // Stage raw bytes already in memory (e.g. generated SPIR-V)
        bool AddRawBytes(const std::string &assetName,
                         std::span<const uint8_t> data,
                         RscAssetType type = RscAssetType::Unknown,
                         RscCompression compression = RscCompression::None,
                         uint8_t shuffleStride = RscByteShuffler::AUTO_STRIDE);

        bool Remove(const std::string &assetName);
        void Clear();
        size_t StagedCount() const { return m_staged.size(); }

        bool Pack(const std::string &outputPath, ProgressFn progress = nullptr) const;

        // Merge existing .rsc + current staged assets → new .rsc
        bool Merge(const std::string &existingRsc, const std::string &outputPath) const;

        static uint64_t HashName(std::string_view name);

    private:
        struct StagedEntry
        {
            std::string assetName;
            std::vector<uint8_t> rawData;
            RscAssetType type;
            RscCompression compression;
            uint8_t shuffleStride;
        };

        // Returns shuffled + compressed + encrypted blob, and fills outEntry fields
        bool ProcessEntry(const StagedEntry &staged,
                          uint64_t entryIndex,
                          const std::array<uint8_t, 32> &key,
                          ResourceEntryDescriptor &outEntry,
                          std::vector<uint8_t> &outBlob) const;

        static std::vector<uint8_t> Compress(std::span<const uint8_t> src,
                                             RscCompression codec);

        RscKeyDeriver::BuildInfo m_buildInfo;
        std::vector<StagedEntry> m_staged;
    };

    // Pack an entire directory tree into one .rsc
    bool PackDirectory(const std::string &sourceDir,
                       const std::string &outputRsc,
                       const RscKeyDeriver::BuildInfo &buildInfo,
                       RscCompression defaultCompression = RscCompression::LZ4,
                       std::function<RscAssetType(std::string_view ext)> fileTyper = nullptr);

    // Extract all assets to a directory (requires the matching BuildInfo to decrypt)
    bool UnpackRsc(const std::string &rscPath,
                   const std::string &outputDir,
                   const RscKeyDeriver::BuildInfo &buildInfo);

} // namespace SF::Engine
