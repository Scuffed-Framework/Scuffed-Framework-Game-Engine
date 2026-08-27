#include "MountedResourceFile.hpp"
#include "RscCrypto.hpp"

// Decompression back-ends
// #include <lz4.h>
// #include <zstd.h>

#include <LowLevel/FileSystem/File.hpp>
#include <sodium.h>

#include <cassert>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace SF::Engine
{
    uint64_t MountedRscFile::HashName(std::string_view name)
    {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;
        uint64_t hash = FNV_OFFSET;
        for (unsigned char c : name)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    RscKeyDeriver::BuildInfo MountedRscFile::BuildInfoFromHeader(
        const ResourceFileHeader &h) const
    {
        return {h.buildTimestamp, h.engineVersion, h.platformSalt};
    }

    bool MountedRscFile::VerifyArchiveSignature(const ResourceFileHeader &header,
                                                const std::array<uint8_t, 32> &key) const
    {
        // Build the signing payload in one contiguous buffer so we make a
        // single crypto_generichash call, avoids multi-part API complexity.
        const size_t headerSignedBytes = offsetof(ResourceFileHeader, archiveSignature); // 48
        const size_t tableBytes = m_entries.size() * sizeof(ResourceEntryDescriptor);
        const size_t payloadSize = headerSignedBytes + tableBytes;

        std::vector<uint8_t> payload(payloadSize);

        // Copy the header portion that was signed (everything before the sig field)
        std::memcpy(payload.data(), &header, headerSignedBytes);

        // Copy the entry table
        std::memcpy(payload.data() + headerSignedBytes,
                    m_entries.data(), tableBytes);

        // Recompute, keyed with the archive's derived key
        uint8_t computed[16];
        crypto_generichash(computed, sizeof(computed),
                           payload.data(), payload.size(),
                           key.data(), key.size());

        // Constant-time compare, sodium_memcmp returns 0 if equal
        return sodium_memcmp(computed, header.archiveSignature, sizeof(computed)) == 0;
    }

    bool MountedRscFile::Mount(const std::string &path)
    {
        Unmount();

        if (sodium_init() < 0)
            return false;

        File file(path);
        if (!file.Open(FileMode::Read))
            return false;

        ResourceFileHeader header{};
        if (file.Read(&header, sizeof(header)) != sizeof(header))
            return false;

        if (std::memcmp(header.magic, RSC_MAGIC, 4) != 0)
            return false;

        if (header.version != RSC_VERSION)
            return false;

        // header.entryCount is untrusted at this point (file could be
        // corrupt or hand-edited). Reject absurd counts up front so we
        // don't attempt a multi-gigabyte / overflowing resize() below.
        if (header.entryCount > RSC_MAX_ENTRY_COUNT)
            return false;

        // Overflow-safe: entryCount is already capped above, and
        // sizeof(ResourceEntryDescriptor) is a small compile-time constant,
        // so this multiplication cannot overflow size_t on any real target.
        const size_t tableBytes = static_cast<size_t>(header.entryCount) * sizeof(ResourceEntryDescriptor);

        if (header.entryCount > 0)
        {
            file.SetPosition(static_cast<size_t>(header.entryTableOffset));
            m_entries.resize(static_cast<size_t>(header.entryCount));

            if (file.Read(m_entries.data(), tableBytes) != tableBytes)
            {
                m_entries.clear();
                return false;
            }
        }

        auto buildInfo = BuildInfoFromHeader(header);
        auto key = RscKeyDeriver::Derive(buildInfo);

        // Must happen after entry table is loaded so VerifyArchiveSignature
        // can include it in the payload.
        if (!VerifyArchiveSignature(header, key))
        {
            RscKeyDeriver::Wipe(key);
            m_entries.clear();
            return false; // tampered or corrupt
        }

        RscKeyDeriver::Wipe(key);

        m_entryIndex.clear();
        m_entryIndex.reserve(m_entries.size());
        for (size_t i = 0; i < m_entries.size(); ++i)
            m_entryIndex.emplace(m_entries[i].nameHash, i);

        if (!MapFile(path))
        {
            m_entries.clear();
            m_entryIndex.clear();
            return false;
        }

        m_header = header;
        m_filePath = path;
        m_mounted = true;
        return true;
    }

    void MountedRscFile::Unmount()
    {
        UnmapFile();
        sodium_memzero(&m_header, sizeof(m_header));
        m_entries.clear();
        m_entryIndex.clear();
        m_filePath.clear();
        m_mounted = false;
    }

    MountedRscFile::~MountedRscFile()
    {
        Unmount();
    }

    MountedRscFile::MountedRscFile(MountedRscFile &&other) noexcept
    {
        *this = std::move(other);
    }

    MountedRscFile &MountedRscFile::operator=(MountedRscFile &&other) noexcept
    {
        if (this == &other)
            return *this;

        Unmount(); // release anything we currently hold

        m_filePath = std::move(other.m_filePath);
        m_entries = std::move(other.m_entries);
        m_entryIndex = std::move(other.m_entryIndex);
        m_header = other.m_header;
        m_mounted = other.m_mounted;
        m_mappedData = other.m_mappedData;
        m_mappedSize = other.m_mappedSize;

#if defined(_WIN32)
        m_fileHandle = other.m_fileHandle;
        m_mappingHandle = other.m_mappingHandle;
        other.m_fileHandle = nullptr;
        other.m_mappingHandle = nullptr;
#else
        m_fileDescriptor = other.m_fileDescriptor;
        other.m_fileDescriptor = -1;
#endif

        other.m_mappedData = nullptr;
        other.m_mappedSize = 0;
        other.m_mounted = false;
        sodium_memzero(&other.m_header, sizeof(other.m_header));

        return *this;
    }

    bool MountedRscFile::MapFile(const std::string &path)
    {
        UnmapFile();

#if defined(_WIN32)
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0)
        {
            CloseHandle(hFile);
            return false;
        }

        HANDLE hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (hMapping == nullptr)
        {
            CloseHandle(hFile);
            return false;
        }

        void *view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (view == nullptr)
        {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return false;
        }

        m_fileHandle = hFile;
        m_mappingHandle = hMapping;
        m_mappedData = static_cast<const uint8_t *>(view);
        m_mappedSize = static_cast<uint64_t>(size.QuadPart);
        return true;
#else
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
            return false;

        struct stat st{};
        if (fstat(fd, &st) != 0 || st.st_size <= 0)
        {
            close(fd);
            return false;
        }

        void *view = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
        if (view == MAP_FAILED)
        {
            close(fd);
            return false;
        }

        m_fileDescriptor = fd;
        m_mappedData = static_cast<const uint8_t *>(view);
        m_mappedSize = static_cast<uint64_t>(st.st_size);
        return true;
#endif
    }

    void MountedRscFile::UnmapFile()
    {
#if defined(_WIN32)
        if (m_mappedData != nullptr)
            UnmapViewOfFile(m_mappedData);
        if (m_mappingHandle != nullptr)
            CloseHandle(static_cast<HANDLE>(m_mappingHandle));
        if (m_fileHandle != nullptr)
            CloseHandle(static_cast<HANDLE>(m_fileHandle));

        m_mappingHandle = nullptr;
        m_fileHandle = nullptr;
#else
        if (m_mappedData != nullptr && m_mappedSize > 0)
            munmap(const_cast<uint8_t *>(m_mappedData), static_cast<size_t>(m_mappedSize));
        if (m_fileDescriptor >= 0)
            close(m_fileDescriptor);

        m_fileDescriptor = -1;
#endif
        m_mappedData = nullptr;
        m_mappedSize = 0;
    }

    const ResourceEntryDescriptor *MountedRscFile::FindEntry(std::string_view assetPath) const
    {
        return FindEntryByHash(HashName(assetPath));
    }

    const ResourceEntryDescriptor *MountedRscFile::FindEntryByHash(uint64_t hash) const
    {
        auto it = m_entryIndex.find(hash);
        if (it == m_entryIndex.end())
            return nullptr;
        return &m_entries[it->second];
    }

    std::vector<uint8_t> MountedRscFile::ReadAsset(std::string_view assetPath) const
    {
        return ReadAssetByHash(HashName(assetPath));
    }

    std::vector<uint8_t> MountedRscFile::ReadAssetByHash(uint64_t hash) const
    {
        auto it = m_entryIndex.find(hash);
        if (it == m_entryIndex.end())
            return {};
        return DecryptDecompress(m_entries[it->second], static_cast<uint64_t>(it->second));
    }

    std::vector<uint8_t> MountedRscFile::ReadAsset(const ResourceEntryDescriptor &entry,
                                                   uint64_t entryIndex) const
    {
        return DecryptDecompress(entry, entryIndex);
    }

    std::vector<uint8_t> MountedRscFile::DecryptDecompress(
        const ResourceEntryDescriptor &entry,
        uint64_t entryIndex) const
    {
        if (!m_mounted || m_mappedData == nullptr)
            return {};

        // entry.compressedSize / dataOffset ultimately come from the entry
        // table, which is authenticated by VerifyArchiveSignature(), but we
        // still guard against overflow and against a corrupt/edited-but-
        // still-plausible entry pointing outside the mapped region.
        if (entry.compressedSize == 0 || entry.compressedSize > RSC_MAX_ASSET_SIZE)
            return {};

        if (entry.dataOffset > m_mappedSize ||
            entry.compressedSize > m_mappedSize - entry.dataOffset)
        {
            return {}; // would read past end of file
        }

        std::span<const uint8_t> encrypted(m_mappedData + entry.dataOffset,
                                           static_cast<size_t>(entry.compressedSize));

        auto buildInfo = BuildInfoFromHeader(m_header);
        auto key = RscKeyDeriver::Derive(buildInfo);
        auto nonce = RscNonce::Derive(entryIndex, entry.nameHash);

        std::vector<uint8_t> decrypted = RscCipher::Decrypt(encrypted, key, nonce);
        RscKeyDeriver::Wipe(key);

        if (decrypted.empty())
            return {};

        // Cap the claimed uncompressed size before allocating, otherwise a
        // corrupt/malicious entry.uncompressedSize is a trivial decompression
        // bomb (allocate gigabytes for a tiny compressed input).
        if (entry.uncompressedSize > RSC_MAX_ASSET_SIZE)
            return {};

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
                reinterpret_cast<const char *>(decrypted.data()),
                reinterpret_cast<char *>(decompressed.data()),
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
                decrypted.data(), decrypted.size());
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

        if (entry.shuffleStride > 1)
            return RscByteShuffler::Unshuffle(decompressed, entry.shuffleStride);

        return decompressed;
    }

} // namespace SF::Engine