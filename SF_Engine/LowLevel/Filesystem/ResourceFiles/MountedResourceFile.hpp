#pragma once

#include "ResourceFileHeader.hpp"
#include "RscCrypto.hpp"
#include <LowLevel/FileSystem/File.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    //  MountedRscFile
    //
    //  Runtime handle for an open .rsc archive.
    //
    //  Mount() workflow:
    //    1. Read + validate header magic / version
    //    2. Derive the 32-byte ChaCha20 key from BuildInfo embedded in header
    //    3. Load entire entry table into RAM (size-capped against
    //       RSC_MAX_ENTRY_COUNT before allocating)
    //    4. Verify archive-level BLAKE2b-128 signature, KEYED with the
    //       derived key (crypto_generichash keyed mode), this is a MAC, not
    //       a bare checksum, so the entry table/header can't be tampered
    //       with and re-signed without knowing the key
    //    5. Build a nameHash → index lookup table for O(1) FindEntry
    //    6. Memory-map the archive file for zero-copy asset reads
    //    7. Key is wiped from memory after signature verification
    //       (re-derived on each ReadAsset call, keeps key off heap at rest)
    //
    //  ReadAsset() workflow per call:
    //    1. Find entry by hash (unordered_map lookup)
    //    2. Re-derive key
    //    3. Derive per-entry nonce from (entryIndex, nameHash)
    //    4. Read encrypted blob directly from the memory-mapped file
    //       (bounds-checked against the mapping size and per-asset caps)
    //    5. Decrypt + verify Poly1305 tag  → tamper-detect per asset
    //    6. Decompress (LZ4 / ZSTD / None), size-capped against
    //       RSC_MAX_ASSET_SIZE before allocating the output buffer
    //    7. Unshuffle byte-stride pre-pass
    //    8. Wipe key
    //
    //  Thread-safety: Mount() is not re-entrant.
    //                 ReadAsset() is safe to call concurrently, the mapped
    //                 view is read-only and shared; no per-call file handle
    //                 or seek position is involved.
    class MountedRscFile
    {
    public:
        MountedRscFile() = default;
        ~MountedRscFile();

        MountedRscFile(const MountedRscFile &) = delete;
        MountedRscFile &operator=(const MountedRscFile &) = delete;
        MountedRscFile(MountedRscFile &&other) noexcept;
        MountedRscFile &operator=(MountedRscFile &&other) noexcept;

        bool Mount(const std::string &path);
        void Unmount();
        bool IsMounted() const { return m_mounted; }

        const ResourceEntryDescriptor *FindEntry(std::string_view assetPath) const;
        const ResourceEntryDescriptor *FindEntryByHash(uint64_t hash) const;
        std::span<const ResourceEntryDescriptor> GetEntries() const
        {
            return {m_entries.data(), m_entries.size()};
        }

        std::vector<uint8_t> ReadAsset(std::string_view assetPath) const;
        std::vector<uint8_t> ReadAsset(const ResourceEntryDescriptor &entry,
                                       uint64_t entryIndex) const;
        std::vector<uint8_t> ReadAssetByHash(uint64_t hash) const;

        uint64_t GetEntryCount() const { return static_cast<uint64_t>(m_entries.size()); }
        std::string GetFilePath() const { return m_filePath; }

        static uint64_t HashName(std::string_view name);

    private:
        bool VerifyArchiveSignature(const ResourceFileHeader &header,
                                    const std::array<uint8_t, 32> &key) const;

        std::vector<uint8_t> DecryptDecompress(const ResourceEntryDescriptor &entry,
                                               uint64_t entryIndex) const;

        RscKeyDeriver::BuildInfo BuildInfoFromHeader(const ResourceFileHeader &h) const;

        bool MapFile(const std::string &path);
        void UnmapFile();

        std::string m_filePath;
        std::vector<ResourceEntryDescriptor> m_entries;
        std::unordered_map<uint64_t, size_t> m_entryIndex; // nameHash -> index into m_entries
        ResourceFileHeader m_header = {};
        bool m_mounted = false;

        const uint8_t *m_mappedData = nullptr; // view into the mapped file, valid while m_mounted
        uint64_t m_mappedSize = 0;

#if defined(_WIN32)
        void *m_fileHandle = nullptr;    // HANDLE
        void *m_mappingHandle = nullptr; // HANDLE
#else
        int m_fileDescriptor = -1;
#endif
    };

} // namespace SF::Engine