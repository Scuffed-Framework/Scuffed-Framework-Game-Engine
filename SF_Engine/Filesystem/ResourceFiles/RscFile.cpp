#include "RscFile.hpp"
#include "MountedResourceFile.hpp"

// #include <lz4hc.h>
// #include <zstd.h>

#include <Filesystem/File.hpp>
#include <sodium.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace SF::Engine
{
    // =========================================================================
    //  Helpers
    // =========================================================================
    uint64_t RscPacker::HashName(std::string_view name)
    {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME  = 1099511628211ULL;
        uint64_t hash = FNV_OFFSET;
        for (unsigned char c : name)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    // =========================================================================
    //  Constructor
    // =========================================================================
    RscPacker::RscPacker(const RscKeyDeriver::BuildInfo& buildInfo)
        : m_buildInfo(buildInfo)
    {
        if (sodium_init() < 0)
            throw std::runtime_error("libsodium failed to initialise");
    }

    // =========================================================================
    //  Staging
    // =========================================================================
    bool RscPacker::AddFile(const std::string& assetName,
                            const std::string& sourcePath,
                            RscAssetType       type,
                            RscCompression     compression,
                            uint8_t            shuffleStride)
    {
        File src(sourcePath);
        if (!src.Open(FileMode::Read))
            return false;

        std::vector<uint8_t> bytes = src.ReadAllBytes();
        if (bytes.empty())
            return false;

        return AddRawBytes(assetName, bytes, type, compression, shuffleStride);
    }

    bool RscPacker::AddRawBytes(const std::string&       assetName,
                                std::span<const uint8_t> data,
                                RscAssetType             type,
                                RscCompression           compression,
                                uint8_t                  shuffleStride)
    {
        if (assetName.empty() || assetName.size() > 43)
            return false;

        for (const auto& s : m_staged)
            if (s.assetName == assetName)
                return false; // duplicate

        StagedEntry e;
        e.assetName     = assetName;
        e.rawData       = std::vector<uint8_t>(data.begin(), data.end());
        e.type          = type;
        e.compression   = compression;
        e.shuffleStride = shuffleStride;
        m_staged.push_back(std::move(e));
        return true;
    }

    bool RscPacker::Remove(const std::string& assetName)
    {
        auto it = std::remove_if(m_staged.begin(), m_staged.end(),
                                 [&](const StagedEntry& e) {
                                     return e.assetName == assetName;
                                 });
        if (it == m_staged.end())
            return false;
        m_staged.erase(it, m_staged.end());
        return true;
    }

    void RscPacker::Clear() { m_staged.clear(); }

    // =========================================================================
    //  Compress helper
    // =========================================================================
    std::vector<uint8_t> RscPacker::Compress(std::span<const uint8_t> src,
                                              RscCompression            codec)
    {
        switch (codec)
        {
        case RscCompression::LZ4:
        {
#if defined(HAVE_LZ4)
            int maxDst = LZ4_compressBound(static_cast<int>(src.size()));
            std::vector<uint8_t> dst(maxDst);
            int sz = LZ4_compress_HC(
                reinterpret_cast<const char*>(src.data()),
                reinterpret_cast<char*>(dst.data()),
                static_cast<int>(src.size()), maxDst,
                LZ4HC_CLEVEL_DEFAULT);
            if (sz > 0 && static_cast<size_t>(sz) < src.size())
            {
                dst.resize(sz);
                return dst;
            }
#endif
            // Fallthrough: store raw if LZ4 not available or compression grew
            return std::vector<uint8_t>(src.begin(), src.end());
        }

        case RscCompression::ZSTD:
        {
#if defined(HAVE_ZSTD)
            size_t bound = ZSTD_compressBound(src.size());
            std::vector<uint8_t> dst(bound);
            size_t sz = ZSTD_compress(dst.data(), bound,
                                      src.data(), src.size(),
                                      ZSTD_CLEVEL_DEFAULT);
            if (!ZSTD_isError(sz) && sz < src.size())
            {
                dst.resize(sz);
                return dst;
            }
#endif
            return std::vector<uint8_t>(src.begin(), src.end());
        }

        default: // None
            return std::vector<uint8_t>(src.begin(), src.end());
        }
    }

    // =========================================================================
    //  ProcessEntry — shuffle → compress → encrypt one asset
    // =========================================================================
    bool RscPacker::ProcessEntry(const StagedEntry&             staged,
                                 uint64_t                        entryIndex,
                                 const std::array<uint8_t, 32>& key,
                                 ResourceEntryDescriptor&        outEntry,
                                 std::vector<uint8_t>&           outBlob) const
    {
        // 1. Byte-shuffle pre-pass
        uint8_t chosenStride = 0;
        std::vector<uint8_t> shuffled = RscByteShuffler::Shuffle(
            staged.rawData, staged.shuffleStride, chosenStride);

        // 2. Compress
        std::vector<uint8_t> compressed = Compress(shuffled, staged.compression);

        // If compression didn't shrink the data, store raw and mark None
        RscCompression actualCodec = staged.compression;
        if (compressed.size() >= shuffled.size())
        {
            compressed  = std::move(shuffled);   // use shuffled-but-uncompressed
            actualCodec = RscCompression::None;
        }

        // 3. Derive nonce and encrypt
        uint64_t nameHash = HashName(staged.assetName);
        RscNonce nonce    = RscNonce::Derive(entryIndex, nameHash);

        outBlob = RscCipher::Encrypt(compressed, key, nonce);
        if (outBlob.empty())
            return false;

        // 4. Fill descriptor (dataOffset set by caller)
        outEntry.nameHash          = nameHash;
        outEntry.dataOffset        = 0; // filled in by Pack()
        outEntry.compressedSize    = static_cast<uint64_t>(outBlob.size()); // includes 16-byte tag
        outEntry.uncompressedSize  = static_cast<uint64_t>(staged.rawData.size());
        outEntry.assetType         = staged.type;
        outEntry.compression       = actualCodec;
        outEntry.shuffleStride     = chosenStride;
        outEntry.flags             = 0;
        std::memset(outEntry.pad,  0, sizeof(outEntry.pad));
        std::memset(outEntry.pad2, 0, sizeof(outEntry.pad2));
        std::memset(outEntry.name, 0, sizeof(outEntry.name));
        std::strncpy(outEntry.name, staged.assetName.c_str(), sizeof(outEntry.name) - 1);

        return true;
    }

    // =========================================================================
    //  Pack
    // =========================================================================
    bool RscPacker::Pack(const std::string& outputPath, ProgressFn progress) const
    {
        // --- Derive key once for the whole archive ---
        auto key = RscKeyDeriver::Derive(m_buildInfo);

        const uint64_t entryCount      = static_cast<uint64_t>(m_staged.size());
        const uint64_t headerSize      = sizeof(ResourceFileHeader);
        const uint64_t entryTableSize  = entryCount * sizeof(ResourceEntryDescriptor);
        const uint64_t entryTableOff   = headerSize;
        const uint64_t dataRegionOff   = headerSize + entryTableSize;

        // --- Process all assets (shuffle + compress + encrypt) ---
        std::vector<ResourceEntryDescriptor> entries(m_staged.size());
        std::vector<std::vector<uint8_t>>    blobs(m_staged.size());

        uint64_t cursor = dataRegionOff;
        for (size_t i = 0; i < m_staged.size(); ++i)
        {
            if (progress)
                progress(m_staged[i].assetName, i, m_staged.size());

            if (!ProcessEntry(m_staged[i], static_cast<uint64_t>(i),
                              key, entries[i], blobs[i]))
            {
                RscKeyDeriver::Wipe(key);
                return false;
            }

            entries[i].dataOffset = cursor;
            cursor += blobs[i].size();
        }

        // --- Build header (archiveSignature zeroed for now) ---
        ResourceFileHeader header{};
        std::memcpy(header.magic, RSC_MAGIC, 4);
        header.version          = RSC_VERSION;
        header.entryCount       = entryCount;
        header.entryTableOffset = entryTableOff;
        header.dataRegionOffset = dataRegionOff;
        header.buildTimestamp   = m_buildInfo.buildTimestamp;
        header.engineVersion    = m_buildInfo.engineVersion;
        header.platformSalt     = m_buildInfo.platformSalt;
        std::memset(header.archiveSignature, 0, sizeof(header.archiveSignature));

        // --- Compute archive signature over header[0..47] + entry table ---
        {
            const size_t signedHeaderBytes = offsetof(ResourceFileHeader, archiveSignature); // 48
            const size_t tableBytes        = entries.size() * sizeof(ResourceEntryDescriptor);
            std::vector<uint8_t> sigPayload(signedHeaderBytes + tableBytes);

            std::memcpy(sigPayload.data(), &header, signedHeaderBytes);
            std::memcpy(sigPayload.data() + signedHeaderBytes,
                        entries.data(), tableBytes);

            crypto_generichash(header.archiveSignature, sizeof(header.archiveSignature),
                               sigPayload.data(), sigPayload.size(),
                               nullptr, 0);
        }

        RscKeyDeriver::Wipe(key);

        // --- Write file ---
        File out(outputPath);
        if (!out.Open(FileMode::Write))
            return false;

        // Header
        out.Write(&header, sizeof(header));

        // Entry table
        out.SetPosition(static_cast<size_t>(entryTableOff));
        out.Write(entries.data(), entries.size() * sizeof(ResourceEntryDescriptor));

        // Data blobs
        out.SetPosition(static_cast<size_t>(dataRegionOff));
        for (const auto& blob : blobs)
            out.Write(blob.data(), blob.size());

        return static_cast<bool>(out);
    }

    // =========================================================================
    //  Merge
    // =========================================================================
    bool RscPacker::Merge(const std::string& existingRsc,
                          const std::string& outputPath) const
    {
        MountedRscFile mounted;
        if (!mounted.Mount(existingRsc))
            return false;

        RscPacker merged(m_buildInfo);

        uint64_t idx = 0;
        for (const auto& entry : mounted.GetEntries())
        {
            std::vector<uint8_t> data = mounted.ReadAsset(entry, idx++);
            if (!data.empty())
                merged.AddRawBytes(entry.name, data,
                                   entry.assetType, entry.compression,
                                   entry.shuffleStride);
        }

        // New staged assets — skip silently if name already exists from archive
        for (const auto& s : m_staged)
            merged.AddRawBytes(s.assetName, s.rawData,
                               s.type, s.compression, s.shuffleStride);

        return merged.Pack(outputPath);
    }

    // =========================================================================
    //  Free helpers
    // =========================================================================
    static RscAssetType ExtensionToType(std::string_view ext)
    {
        if (ext == ".png"  || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".ktx"  || ext == ".ktx2"|| ext == ".dds")
            return RscAssetType::Texture;
        if (ext == ".mesh" || ext == ".obj" || ext == ".fbx"  || ext == ".gltf")
            return RscAssetType::Mesh;
        if (ext == ".spv")
            return RscAssetType::Shader;
        if (ext == ".wav"  || ext == ".ogg" || ext == ".mp3"  || ext == ".flac")
            return RscAssetType::Audio;
        if (ext == ".mat")
            return RscAssetType::Material;
        if (ext == ".lua"  || ext == ".js")
            return RscAssetType::Script;
        if (ext == ".ttf"  || ext == ".otf")
            return RscAssetType::Font;
        return RscAssetType::Unknown;
    }

    bool PackDirectory(const std::string&              sourceDir,
                       const std::string&              outputRsc,
                       const RscKeyDeriver::BuildInfo& buildInfo,
                       RscCompression                  defaultCompression,
                       std::function<RscAssetType(std::string_view)> fileTyper)
    {
        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir))
            return false;

        RscPacker packer(buildInfo);

        for (const auto& dirEntry : fs::recursive_directory_iterator(sourceDir))
        {
            if (!dirEntry.is_regular_file())
                continue;

            std::string fullPath = dirEntry.path().string();
            std::string relPath  = fs::relative(dirEntry.path(), sourceDir).string();
            std::replace(relPath.begin(), relPath.end(), '\\', '/');

            std::string ext  = dirEntry.path().extension().string();
            RscAssetType type = fileTyper ? fileTyper(ext) : ExtensionToType(ext);

            packer.AddFile(relPath, fullPath, type, defaultCompression);
        }

        return packer.Pack(outputRsc);
    }

    bool UnpackRsc(const std::string&              rscPath,
                   const std::string&              outputDir,
                   const RscKeyDeriver::BuildInfo& buildInfo)
    {
        // buildInfo is embedded in the header, but we accept it as a parameter
        // to allow cross-build extraction during development. The Mount() will
        // verify the archive signature using the header's own fields.
        (void)buildInfo;

        MountedRscFile mounted;
        if (!mounted.Mount(rscPath))
            return false;

        fs::create_directories(outputDir);

        uint64_t idx = 0;
        for (const auto& entry : mounted.GetEntries())
        {
            std::vector<uint8_t> data = mounted.ReadAsset(entry, idx++);
            if (data.empty())
                continue;

            fs::path outPath = fs::path(outputDir) / entry.name;
            fs::create_directories(outPath.parent_path());

            File outFile(outPath.string());
            if (!outFile.Open(FileMode::Write))
                continue;

            outFile.WriteAllBytes(data);
        }

        return true;
    }

} // namespace SF::Engine
