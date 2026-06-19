#include "MountedResourceFile.hpp"
#include "RscCrypto.hpp"

// Decompression back-ends
// #include <lz4.h>
// #include <zstd.h>

#include <Filesystem/File.hpp>
#include <sodium.h>

#include <cassert>
#include <cstring>

namespace SF::Engine
{
    // =========================================================================
    //  Hash — FNV-1a 64-bit
    // =========================================================================
    uint64_t MountedRscFile::HashName(std::string_view name)
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
    //  BuildInfo helpers
    // =========================================================================
    RscKeyDeriver::BuildInfo MountedRscFile::BuildInfoFromHeader(
        const ResourceFileHeader& h) const
    {
        return { h.buildTimestamp, h.engineVersion, h.platformSalt };
    }

    // =========================================================================
    //  Archive-level signature verification
    //
    //  Recomputes BLAKE2b-128 over:
    //    header bytes [0..47]  (everything before archiveSignature field)
    //  + raw entry table bytes
    //  Compares against header.archiveSignature using a constant-time compare.
    // =========================================================================
    bool MountedRscFile::VerifyArchiveSignature(const ResourceFileHeader& header) const
    {
        // Build the signing payload in one contiguous buffer so we make a
        // single crypto_generichash call — avoids multi-part API complexity.
        const size_t headerSignedBytes = offsetof(ResourceFileHeader, archiveSignature); // 48
        const size_t tableBytes        = m_entries.size() * sizeof(ResourceEntryDescriptor);
        const size_t payloadSize       = headerSignedBytes + tableBytes;

        std::vector<uint8_t> payload(payloadSize);

        // Copy the header portion that was signed (everything before the sig field)
        std::memcpy(payload.data(), &header, headerSignedBytes);

        // Copy the entry table
        std::memcpy(payload.data() + headerSignedBytes,
                    m_entries.data(), tableBytes);

        // Recompute
        uint8_t computed[16];
        crypto_generichash(computed, sizeof(computed),
                           payload.data(), payload.size(),
                           nullptr, 0);

        // Constant-time compare — sodium_memcmp returns 0 if equal
        return sodium_memcmp(computed, header.archiveSignature, sizeof(computed)) == 0;
    }

    // =========================================================================
    //  Mount
    // =========================================================================
    bool MountedRscFile::Mount(const std::string& path)
    {
        Unmount();

        if (sodium_init() < 0)
            return false;

        File file(path);
        if (!file.Open(FileMode::Read))
            return false;

        // --- Read + validate header ---
        ResourceFileHeader header{};
        if (file.Read(&header, sizeof(header)) != sizeof(header))
            return false;

        if (std::memcmp(header.magic, RSC_MAGIC, 4) != 0)
            return false;

        if (header.version != RSC_VERSION)
            return false;

        // --- Load entry table into RAM ---
        if (header.entryCount > 0)
        {
            file.SetPosition(static_cast<size_t>(header.entryTableOffset));
            m_entries.resize(static_cast<size_t>(header.entryCount));

            size_t tableBytes = header.entryCount * sizeof(ResourceEntryDescriptor);
            if (file.Read(m_entries.data(), tableBytes) != tableBytes)
            {
                m_entries.clear();
                return false;
            }
        }

        // --- Verify archive signature (header + entry table) ---
        // Must happen after entry table is loaded so VerifyArchiveSignature
        // can include it in the payload.
        if (!VerifyArchiveSignature(header))
        {
            m_entries.clear();
            return false; // tampered or corrupt
        }

        m_header   = header;
        m_filePath = path;
        m_mounted  = true;
        return true;
    }

    void MountedRscFile::Unmount()
    {
        sodium_memzero(&m_header, sizeof(m_header));
        m_entries.clear();
        m_filePath.clear();
        m_mounted = false;
    }

    // =========================================================================
    //  Lookup
    // =========================================================================
    const ResourceEntryDescriptor* MountedRscFile::FindEntry(std::string_view assetPath) const
    {
        return FindEntryByHash(HashName(assetPath));
    }

    const ResourceEntryDescriptor* MountedRscFile::FindEntryByHash(uint64_t hash) const
    {
        for (const auto& e : m_entries)
            if (e.nameHash == hash)
                return &e;
        return nullptr;
    }

    // =========================================================================
    //  Public ReadAsset overloads
    // =========================================================================
    std::vector<uint8_t> MountedRscFile::ReadAsset(std::string_view assetPath) const
    {
        uint64_t hash = HashName(assetPath);
        for (uint64_t i = 0; i < m_entries.size(); ++i)
            if (m_entries[i].nameHash == hash)
                return DecryptDecompress(m_entries[i], i);
        return {};
    }

    std::vector<uint8_t> MountedRscFile::ReadAssetByHash(uint64_t hash) const
    {
        for (uint64_t i = 0; i < m_entries.size(); ++i)
            if (m_entries[i].nameHash == hash)
                return DecryptDecompress(m_entries[i], i);
        return {};
    }

    std::vector<uint8_t> MountedRscFile::ReadAsset(const ResourceEntryDescriptor& entry,
                                                    uint64_t entryIndex) const
    {
        return DecryptDecompress(entry, entryIndex);
    }

    // =========================================================================
    //  DecryptDecompress — full pipeline reversal for one asset
    // =========================================================================
    std::vector<uint8_t> MountedRscFile::DecryptDecompress(
        const ResourceEntryDescriptor& entry,
        uint64_t                        entryIndex) const
    {
        if (!m_mounted)
            return {};

        // --- 1. Read encrypted blob from disk ---
        // Open a fresh File handle per call so concurrent reads don't race
        // on a shared seek position.
        File file(m_filePath);
        if (!file.Open(FileMode::Read))
            return {};

        file.SetPosition(static_cast<size_t>(entry.dataOffset));

        std::vector<uint8_t> encrypted(static_cast<size_t>(entry.compressedSize));
        if (file.Read(encrypted.data(), encrypted.size()) != encrypted.size())
            return {};

        // --- 2. Derive key + nonce, then decrypt ---
        auto buildInfo = BuildInfoFromHeader(m_header);
        auto key       = RscKeyDeriver::Derive(buildInfo);
        auto nonce     = RscNonce::Derive(entryIndex, entry.nameHash);

        std::vector<uint8_t> decrypted = RscCipher::Decrypt(encrypted, key, nonce);
        RscKeyDeriver::Wipe(key);

        // Empty = tag mismatch → asset was tampered with or key is wrong
        if (decrypted.empty())
            return {};

        // --- 3. Decompress ---
        std::vector<uint8_t> decompressed;

        switch (entry.compression)
        {
        case RscCompression::None:
            decompressed = std::move(decrypted);
            break;

        case RscCompression::LZ4:
        {
#if defined(HAVE_LZ4)
            decompressed.resize(static_cast<size_t>(entry.uncompressedSize));
            int r = LZ4_decompress_safe(
                reinterpret_cast<const char*>(decrypted.data()),
                reinterpret_cast<char*>(decompressed.data()),
                static_cast<int>(decrypted.size()),
                static_cast<int>(decompressed.size()));
            if (r < 0)
                return {};
#else
            decompressed = std::move(decrypted); // stub
#endif
            break;
        }

        case RscCompression::ZSTD:
        {
#if defined(HAVE_ZSTD)
            decompressed.resize(static_cast<size_t>(entry.uncompressedSize));
            size_t r = ZSTD_decompress(
                decompressed.data(), decompressed.size(),
                decrypted.data(),    decrypted.size());
            if (ZSTD_isError(r))
                return {};
#else
            decompressed = std::move(decrypted); // stub
#endif
            break;
        }

        default:
            return {};
        }

        // --- 4. Undo byte-shuffle pre-pass ---
        if (entry.shuffleStride > 1)
            return RscByteShuffler::Unshuffle(decompressed, entry.shuffleStride);

        return decompressed;
    }

} // namespace SF::Engine
