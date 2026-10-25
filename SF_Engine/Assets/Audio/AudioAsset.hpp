#pragma once

#include <Assets/AssetPipeline.hpp>
#include <Assets/Audio/Audio.hpp>
#include <Assets/Audio/SoundBuffer.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <memory>

namespace SF::Engine
{
    struct AudioImportSettings
    {
        float defaultGain = 1.0f;
        bool defaultLoop  = false;
        int busType       = static_cast<int>(Audio::Type::General); // Audio::Type
    };

    // Decoded-in-memory sound (OpenAL buffer). Streaming is not implemented yet.
    class AudioAsset : public AssetBase
    {
        SF_RTTI(AudioAsset, AssetBase)
    public:
        AudioAsset() { type = AssetType::Audio; }

        AudioImportSettings settings;

        // Valid after EnsureLoaded().
        std::shared_ptr<SoundBuffer> buffer;
        float durationSeconds = 0.0f;
        int channels          = 0;
        int sampleRate        = 0;

        bool EnsureLoaded() override;
        void Save() override { SaveMeta(); }
        bool Load(span<const uint8_t> /*payload*/) override { return EnsureLoaded(); }

        void SerializeImportSettings(XMLNode &node) const override;
        void DeserializeImportSettings(const XMLNode &node) override;

    private:
        // SoundBuffer keeps a raw File* in its DataInput, so the asset owns the File for the buffer's lifetime.
        std::unique_ptr<File> file_;
    };
} // namespace SF::Engine
