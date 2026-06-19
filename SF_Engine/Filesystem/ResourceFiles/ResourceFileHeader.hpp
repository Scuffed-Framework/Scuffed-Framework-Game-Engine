#pragma once

#include <cstdint>

namespace SF::Engine
{
    // -------------------------------------------------------------------------
    //  .rsc binary layout  (all fields little-endian)
    //
    //  [ResourceFileHeader]           – 64 bytes, always at offset 0
    //  [ResourceEntryDescriptor] × N  – entry table at header.entryTableOffset
    //  [encrypted + tagged blobs]     – asset data at header.dataRegionOffset
    //
    //  Pipeline (pack):
    //    raw bytes
    //      → RscByteShuffler::Shuffle()       (custom pre-pass, stride stored per entry)
    //      → LZ4 / ZSTD / None               (stored per entry)
    //      → RscCipher::Encrypt()             (ChaCha20-Poly1305, tag appended)
    //
    //  Pipeline (unpack):
    //    encrypted blob
    //      → RscCipher::Decrypt()             (verifies Poly1305 tag — tamper check)
    //      → LZ4 / ZSTD / None decompress
    //      → RscByteShuffler::Unshuffle()
    //      → raw bytes
    //
    //  Archive-level integrity:
    //    header.archiveSignature = BLAKE2b-128( header[0..47] || entry_table )
    //    Verified by MountedRscFile::Mount() before trusting any entry offsets.
    // -------------------------------------------------------------------------

    static constexpr char     RSC_MAGIC[4] = { 'S', 'R', 'S', 'C' };
    static constexpr uint32_t RSC_VERSION  = 2;

    enum class RscCompression : uint8_t
    {
        None = 0,
        LZ4  = 1,
        ZSTD = 2,
    };

    enum class RscAssetType : uint16_t
    {
        Unknown  = 0,
        Texture  = 1,
        Mesh     = 2,
        Shader   = 3,
        Audio    = 4,
        Material = 5,
        Script   = 6,
        Font     = 7,
    };

    // -------------------------------------------------------------------------
    //  File header — 64 bytes
    // -------------------------------------------------------------------------
    #pragma pack(push, 1)
    struct ResourceFileHeader
    {
        char     magic[4];              //  0  "SRSC"
        uint32_t version;               //  4  RSC_VERSION
        uint64_t entryCount;            //  8  number of ResourceEntryDescriptor records
        uint64_t entryTableOffset;      // 16  byte offset to entry table
        uint64_t dataRegionOffset;      // 24  byte offset to first encrypted blob
        uint64_t buildTimestamp;        // 32  Unix seconds — part of Half B key derivation
        uint32_t engineVersion;         // 40  (major<<16)|(minor<<8)|patch
        uint32_t platformSalt;          // 44  hash of target OS/arch string
        uint8_t  archiveSignature[16];  // 48  BLAKE2b-128( header[0..47] || entry table )
                                        //     zeroed before computing, then written back
        // Total: 4+4+8+8+8+8+4+4+16 = 64
    };

    // -------------------------------------------------------------------------
    //  Per-asset entry — 96 bytes
    // -------------------------------------------------------------------------
    struct ResourceEntryDescriptor
    {
        uint64_t       nameHash;          //  0  FNV-1a 64 of asset path
        uint64_t       dataOffset;        //  8  absolute byte offset in .rsc
        uint64_t       compressedSize;    // 16  bytes on disk (post-encrypt, includes 16-byte Poly1305 tag)
        uint64_t       uncompressedSize;  // 24  bytes after full pipeline reversal
        RscAssetType   assetType;         // 32
        RscCompression compression;       // 34
        uint8_t        shuffleStride;     // 35  stride used by RscByteShuffler (1 = disabled)
        uint8_t        flags;             // 36  reserved, must be 0
        uint8_t        pad[7];            // 37
        char           name[44];          // 44  null-terminated asset path (43 chars max)
        uint8_t        pad2[8];           // 88
        // Total: 96
    };
    #pragma pack(pop)

    static_assert(sizeof(ResourceFileHeader)      == 64, "Header size mismatch");
    static_assert(sizeof(ResourceEntryDescriptor) == 96, "Entry size mismatch");

} // namespace SF::Engine
