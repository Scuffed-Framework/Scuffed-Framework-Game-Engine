#pragma once

#include "ResourceFileHeader.hpp"
#include "RscCrypto.hpp"
#include <Filesystem/File.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SF::Engine
{
    // -------------------------------------------------------------------------
    //  MountedRscFile
    //
    //  Runtime handle for an open .rsc archive.
    //
    //  Mount() workflow:
    //    1. Read + validate header magic / version
    //    2. Derive the 32-byte ChaCha20 key from BuildInfo embedded in header
    //    3. Verify archive-level BLAKE2b-128 signature (tamper check on header
    //       + entry table — catches swapped/injected entries before any seek)
    //    4. Load entire entry table into RAM
    //    5. Key is wiped from memory after signature verification
    //       (re-derived on each ReadAsset call — keeps key off heap at rest)
    //
    //  ReadAsset() workflow per call:
    //    1. Find entry by hash
    //    2. Re-derive key
    //    3. Derive per-entry nonce from (entryIndex, nameHash)
    //    4. Read encrypted blob from disk
    //    5. Decrypt + verify Poly1305 tag  → tamper-detect per asset
    //    6. Decompress (LZ4 / ZSTD / None)
    //    7. Unshuffle byte-stride pre-pass
    //    8. Wipe key
    //
    //  Thread-safety: Mount() is not re-entrant.
    //                 ReadAsset() is safe to call concurrently (fresh File + key per call).
    // -------------------------------------------------------------------------
    class MountedRscFile
    {
    public:
        MountedRscFile() = default;
        ~MountedRscFile() = default;

        MountedRscFile(const MountedRscFile&)            = delete;
        MountedRscFile& operator=(const MountedRscFile&) = delete;
        MountedRscFile(MountedRscFile&&)                 = default;
        MountedRscFile& operator=(MountedRscFile&&)      = default;

        // ------------------------------------------------------------------
        //  Mount / unmount
        // ------------------------------------------------------------------
        bool Mount(const std::string& path);
        void Unmount();
        bool IsMounted() const { return m_mounted; }

        // ------------------------------------------------------------------
        //  Entry lookup
        // ------------------------------------------------------------------
        const ResourceEntryDescriptor* FindEntry(std::string_view assetPath) const;
        const ResourceEntryDescriptor* FindEntryByHash(uint64_t hash) const;
        std::span<const ResourceEntryDescriptor> GetEntries() const
        {
            return { m_entries.data(), m_entries.size() };
        }

        // ------------------------------------------------------------------
        //  Asset loading — returns decompressed, decrypted raw bytes.
        //  Empty vector = not found, decryption failure, or tamper detected.
        // ------------------------------------------------------------------
        std::vector<uint8_t> ReadAsset(std::string_view assetPath) const;
        std::vector<uint8_t> ReadAsset(const ResourceEntryDescriptor& entry,
                                       uint64_t entryIndex) const;
        std::vector<uint8_t> ReadAssetByHash(uint64_t hash) const;

        // ------------------------------------------------------------------
        //  Metadata
        // ------------------------------------------------------------------
        uint64_t    GetEntryCount()   const { return static_cast<uint64_t>(m_entries.size()); }
        std::string GetFilePath()     const { return m_filePath; }

        static uint64_t HashName(std::string_view name);

    private:
        bool VerifyArchiveSignature(const ResourceFileHeader& header) const;

        std::vector<uint8_t> DecryptDecompress(const ResourceEntryDescriptor& entry,
                                               uint64_t                        entryIndex) const;

        RscKeyDeriver::BuildInfo BuildInfoFromHeader(const ResourceFileHeader& h) const;

        std::string                          m_filePath;
        std::vector<ResourceEntryDescriptor> m_entries;
        ResourceFileHeader                   m_header   = {};
        bool                                 m_mounted  = false;
    };

} // namespace SF::Engine
