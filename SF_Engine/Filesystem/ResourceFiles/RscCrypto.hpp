#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <sodium.h>

namespace SF::Engine
{
    //  Compile-time obfuscation of Half A
    //
    //  KEY_HALF_A_RAW  , your actual 16 secret bytes, change before shipping
    //  KEY_HALF_A_MASK , XOR mask applied at compile time; raw never appears
    //                     in the binary as plaintext
    //
    //  At runtime, UnmaskHalfA() recovers the real bytes.
    //  Keep both constants in a translation unit that strips debug info.
    namespace detail
    {
        // !! CHANGE THESE BEFORE SHIPPING !!
        inline constexpr uint8_t KEY_HALF_A_RAW[16] = {
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
            0xF0, 0x0D, 0xFA, 0xCE, 0x12, 0x34, 0x56, 0x78};
        inline constexpr uint8_t KEY_HALF_A_MASK[16] = {
            0x5A, 0x3C, 0x71, 0x88, 0xA1, 0x0F, 0x2D, 0xE9,
            0x44, 0xBB, 0x91, 0x67, 0xC3, 0x55, 0x0A, 0xF2};
        // What is actually stored in the binary (XOR of raw ^ mask)
        inline constexpr uint8_t KEY_HALF_A_STORED[16] = {
            KEY_HALF_A_RAW[0] ^ KEY_HALF_A_MASK[0],
            KEY_HALF_A_RAW[1] ^ KEY_HALF_A_MASK[1],
            KEY_HALF_A_RAW[2] ^ KEY_HALF_A_MASK[2],
            KEY_HALF_A_RAW[3] ^ KEY_HALF_A_MASK[3],
            KEY_HALF_A_RAW[4] ^ KEY_HALF_A_MASK[4],
            KEY_HALF_A_RAW[5] ^ KEY_HALF_A_MASK[5],
            KEY_HALF_A_RAW[6] ^ KEY_HALF_A_MASK[6],
            KEY_HALF_A_RAW[7] ^ KEY_HALF_A_MASK[7],
            KEY_HALF_A_RAW[8] ^ KEY_HALF_A_MASK[8],
            KEY_HALF_A_RAW[9] ^ KEY_HALF_A_MASK[9],
            KEY_HALF_A_RAW[10] ^ KEY_HALF_A_MASK[10],
            KEY_HALF_A_RAW[11] ^ KEY_HALF_A_MASK[11],
            KEY_HALF_A_RAW[12] ^ KEY_HALF_A_MASK[12],
            KEY_HALF_A_RAW[13] ^ KEY_HALF_A_MASK[13],
            KEY_HALF_A_RAW[14] ^ KEY_HALF_A_MASK[14],
            KEY_HALF_A_RAW[15] ^ KEY_HALF_A_MASK[15],
        };
    } // namespace detail

    //  RscKeyDeriver
    //
    //  Derives the final 32-byte ChaCha20 key from two halves:
    //
    //    Half A (16 bytes), unmasked from the obfuscated binary constant
    //    Half B (16 bytes), BLAKE2b( buildTimestamp || engineVersion || platform )
    //    Final  (32 bytes), sodium_memzero-safe concat: [HalfA | HalfB]
    //
    //  The two halves are XOR'd into their respective slots rather than
    //  concatenated so that neither half appears verbatim in memory.
    class RscKeyDeriver
    {
    public:
        // Build parameters baked in by the build system / CMake configure step.
        // Pack these into the build with -DRSC_BUILD_TIMESTAMP=... etc.
        struct BuildInfo
        {
            uint64_t buildTimestamp; // Unix seconds at pack time
            uint32_t engineVersion;  // e.g. (major<<16)|(minor<<8)|patch
            uint32_t platformSalt;   // e.g. hash of target OS + arch string
        };

        // Derive and return the 32-byte key.
        // Returned array should be sodium_memzero'd after use.
        static std::array<uint8_t, 32> Derive(const BuildInfo &info);

        // Wipe a key buffer from memory (wraps sodium_memzero)
        static void Wipe(std::array<uint8_t, 32> &key);

    private:
        static void UnmaskHalfA(uint8_t out[16]);
        static void DeriveHalfB(const BuildInfo &info, uint8_t out[16]);
    };

    //  RscNonce
    //
    //  12-byte IETF ChaCha20-Poly1305 nonce derived deterministically from the
    //  entry index and name hash, no stored randomness needed.
    //
    //  nonce = BLAKE2b-96( entryIndex || nameHash )
    //  (96-bit output = exactly crypto_aead_chacha20poly1305_IETF_NPUBBYTES)
    struct RscNonce
    {
        uint8_t bytes[crypto_aead_chacha20poly1305_ietf_NPUBBYTES]; // 12

        static RscNonce Derive(uint64_t entryIndex, uint64_t nameHash);
    };

    //  RscByteShuffler
    //
    //  Custom pre-compression pass:
    //
    //  Forward (before compress):
    //    Split the input into N strides of width W.
    //    Reorder so all byte[0]s come first, then all byte[1]s, etc.
    //    This clusters similar-magnitude bytes together, which dramatically
    //    improves LZ4/ZSTD ratio on float arrays, indices, and structs.
    //    It also makes the stream unrecognizable to format sniffers.
    //
    //  Inverse (after decompress):
    //    Reverse the stride-split back to interleaved layout.
    //
    //  stride is written into the ResourceEntryDescriptor.flags so the
    //  decompressor knows how to undo it without guessing.
    class RscByteShuffler
    {
    public:
        // Choose a good stride for the data type.  Pass 0 to auto-select.
        static constexpr uint8_t AUTO_STRIDE = 0;

        // Returns the shuffled bytes and writes the chosen stride to outStride
        static std::vector<uint8_t> Shuffle(std::span<const uint8_t> src,
                                            uint8_t stride,
                                            uint8_t &outStride);

        // Undoes Shuffle, stride must match what Shuffle produced
        static std::vector<uint8_t> Unshuffle(std::span<const uint8_t> src,
                                              uint8_t stride);

    private:
        static uint8_t PickStride(size_t dataSize);
    };

    //  RscCipher
    //
    //  Thin wrapper around libsodium ChaCha20-Poly1305 IETF.
    //
    //  Encrypt:  plaintext → ciphertext || tag  (ciphertext.size() == plaintext.size())
    //  Decrypt:  ciphertext || tag → plaintext, returns false if tag invalid
    //
    //  The 16-byte Poly1305 tag is appended to the ciphertext in the .rsc blob.
    //  compressedSize in the entry descriptor includes the tag bytes.
    class RscCipher
    {
    public:
        static constexpr size_t TAG_BYTES = crypto_aead_chacha20poly1305_ietf_ABYTES; // 16

        // Encrypt plaintext in-place, appending the 16-byte tag.
        // output.size() == input.size() + TAG_BYTES
        static std::vector<uint8_t> Encrypt(std::span<const uint8_t> plaintext,
                                            const std::array<uint8_t, 32> &key,
                                            const RscNonce &nonce);

        // Decrypt and authenticate.  Returns empty vector on tag mismatch.
        static std::vector<uint8_t> Decrypt(std::span<const uint8_t> ciphertext, // includes tag
                                            const std::array<uint8_t, 32> &key,
                                            const RscNonce &nonce);
    };

} // namespace SF::Engine
