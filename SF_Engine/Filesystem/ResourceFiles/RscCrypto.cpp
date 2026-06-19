#include "RscCrypto.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace SF::Engine
{
    // =========================================================================
    //  RscKeyDeriver
    // =========================================================================

    void RscKeyDeriver::UnmaskHalfA(uint8_t out[16])
    {
        // Re-apply the mask at runtime to recover the original bytes.
        // Compiler will likely inline this as 16 XOR immediates — no loop visible
        // in the disassembly that screams "decoding key".
        for (int i = 0; i < 16; ++i)
            out[i] = detail::KEY_HALF_A_STORED[i] ^ detail::KEY_HALF_A_MASK[i];
    }

    void RscKeyDeriver::DeriveHalfB(const BuildInfo& info, uint8_t out[16])
    {
        // Pack build parameters into a 16-byte input block
        uint8_t input[16];
        std::memcpy(input + 0, &info.buildTimestamp, 8);
        std::memcpy(input + 8, &info.engineVersion,  4);
        std::memcpy(input + 12, &info.platformSalt,  4);

        // BLAKE2b with a 16-byte output (generic hash, no key)
        crypto_generichash(out, 16, input, sizeof(input), nullptr, 0);

        sodium_memzero(input, sizeof(input));
    }

    std::array<uint8_t, 32> RscKeyDeriver::Derive(const BuildInfo& info)
    {
        if (sodium_init() < 0)
            throw std::runtime_error("libsodium failed to initialise");

        uint8_t halfA[16];
        uint8_t halfB[16];

        UnmaskHalfA(halfA);
        DeriveHalfB(info, halfB);

        std::array<uint8_t, 32> key;
        // Store as [halfA | halfB] — neither half appears as a contiguous
        // 16-byte block because they're in separate local arrays that get
        // copied into the key only here.
        std::memcpy(key.data() + 0,  halfA, 16);
        std::memcpy(key.data() + 16, halfB, 16);

        sodium_memzero(halfA, 16);
        sodium_memzero(halfB, 16);

        return key;
    }

    void RscKeyDeriver::Wipe(std::array<uint8_t, 32>& key)
    {
        sodium_memzero(key.data(), key.size());
    }

    // =========================================================================
    //  RscNonce
    // =========================================================================

    RscNonce RscNonce::Derive(uint64_t entryIndex, uint64_t nameHash)
    {
        // Pack two 64-bit values → 16-byte input, hash down to 12-byte nonce
        uint8_t input[16];
        std::memcpy(input + 0, &entryIndex, 8);
        std::memcpy(input + 8, &nameHash,   8);

        RscNonce nonce;
        crypto_generichash(nonce.bytes,
                           sizeof(nonce.bytes),  // 12 bytes out
                           input, sizeof(input),
                           nullptr, 0);

        sodium_memzero(input, sizeof(input));
        return nonce;
    }

    // =========================================================================
    //  RscByteShuffler
    // =========================================================================

    uint8_t RscByteShuffler::PickStride(size_t dataSize)
    {
        // Heuristic: most engine assets are either:
        //   float arrays    → stride 4  (groups exponent bytes, mantissa bytes…)
        //   uint16 indices  → stride 2
        //   raw bytes/audio → stride 1  (no-op)
        // We pick 4 unless the data is very small.
        if (dataSize < 8)  return 1;
        if (dataSize < 64) return 2;
        return 4;
    }

    std::vector<uint8_t> RscByteShuffler::Shuffle(std::span<const uint8_t> src,
                                                   uint8_t                   stride,
                                                   uint8_t&                  outStride)
    {
        if (stride == AUTO_STRIDE)
            stride = PickStride(src.size());

        outStride = stride;

        if (stride <= 1 || src.size() < static_cast<size_t>(stride))
        {
            // Nothing to reorder
            outStride = 1;
            return std::vector<uint8_t>(src.begin(), src.end());
        }

        const size_t total     = src.size();
        const size_t fullRows  = total / stride;
        const size_t remainder = total % stride;

        std::vector<uint8_t> out(total);

        // Interleaved layout  [A0 B0 C0 D0 | A1 B1 C1 D1 | ...]
        //  → Strided layout   [A0 A1 A2 ... | B0 B1 B2 ... | ...]
        for (size_t bytePos = 0; bytePos < stride; ++bytePos)
        {
            size_t outBase = bytePos * fullRows;
            for (size_t row = 0; row < fullRows; ++row)
                out[outBase + row] = src[row * stride + bytePos];
        }

        // Tail bytes that didn't fill a full stride chunk — append verbatim
        size_t tailSrc = fullRows * stride;
        size_t tailDst = stride * fullRows;
        for (size_t i = 0; i < remainder; ++i)
            out[tailDst + i] = src[tailSrc + i];

        return out;
    }

    std::vector<uint8_t> RscByteShuffler::Unshuffle(std::span<const uint8_t> src,
                                                     uint8_t                   stride)
    {
        if (stride <= 1 || src.size() < static_cast<size_t>(stride))
            return std::vector<uint8_t>(src.begin(), src.end());

        const size_t total     = src.size();
        const size_t fullRows  = total / stride;
        const size_t remainder = total % stride;

        std::vector<uint8_t> out(total);

        for (size_t bytePos = 0; bytePos < stride; ++bytePos)
        {
            size_t inBase = bytePos * fullRows;
            for (size_t row = 0; row < fullRows; ++row)
                out[row * stride + bytePos] = src[inBase + row];
        }

        size_t tailSrc = stride * fullRows;
        size_t tailDst = fullRows * stride;
        for (size_t i = 0; i < remainder; ++i)
            out[tailDst + i] = src[tailSrc + i];

        return out;
    }

    // =========================================================================
    //  RscCipher
    // =========================================================================

    std::vector<uint8_t> RscCipher::Encrypt(std::span<const uint8_t>       plaintext,
                                             const std::array<uint8_t, 32>& key,
                                             const RscNonce&                 nonce)
    {
        std::vector<uint8_t> out(plaintext.size() + TAG_BYTES);
        unsigned long long   outLen = 0;

        int rc = crypto_aead_chacha20poly1305_ietf_encrypt(
            out.data(), &outLen,
            plaintext.data(), static_cast<unsigned long long>(plaintext.size()),
            nullptr, 0,           // no additional data
            nullptr,              // nsec (unused by this construction)
            nonce.bytes,
            key.data());

        if (rc != 0)
        {
            sodium_memzero(out.data(), out.size());
            return {};
        }

        out.resize(static_cast<size_t>(outLen));
        return out;
    }

    std::vector<uint8_t> RscCipher::Decrypt(std::span<const uint8_t>       ciphertext,
                                             const std::array<uint8_t, 32>& key,
                                             const RscNonce&                 nonce)
    {
        if (ciphertext.size() < TAG_BYTES)
            return {};

        std::vector<uint8_t> out(ciphertext.size() - TAG_BYTES);
        unsigned long long   outLen = 0;

        int rc = crypto_aead_chacha20poly1305_ietf_decrypt(
            out.data(), &outLen,
            nullptr,              // nsec
            ciphertext.data(), static_cast<unsigned long long>(ciphertext.size()),
            nullptr, 0,           // no additional data
            nonce.bytes,
            key.data());

        if (rc != 0)
        {
            // Tag mismatch — file was tampered with or key is wrong
            sodium_memzero(out.data(), out.size());
            return {};
        }

        out.resize(static_cast<size_t>(outLen));
        return out;
    }

} // namespace SF::Engine
