#include "AudioAsset.hpp"

#include <Engine/Log/Log.hpp>

// Loaders register themselves from their headers (inline static members); include them so every
// supported extension is registered in any build that has audio import.
#include <Assets/Audio/Flac/FlacSoundBuffer.hpp>
#include <Assets/Audio/Mpeg/Mp3SoundBuffer.hpp>
#include <Assets/Audio/Ogg/OggSoundBuffer.hpp>
#include <Assets/Audio/Opus/OpusSoundBuffer.hpp>
#include <Assets/Audio/Wav/WaveSoundBuffer.hpp>

#ifdef _PLATFORM_MACOS
    #include <OpenAL/al.h>
#else
    #include <al.h>
#endif

#include <filesystem>

namespace SF::Engine
{
    bool AudioAsset::EnsureLoaded()
    {
        if (buffer)
            return true;

        if (assetPath.empty() || !std::filesystem::exists(assetPath))
        {
            Log::Error("AudioAsset '{}': source file is missing: {}", name, assetPath.string());
            return false;
        }

        file_ = std::make_unique<File>(assetPath);

        DataInput input;
        input.file = file_.get();

        auto created = SoundBuffer::Create(input);
        if (!created || created->GetBuffer() == 0)
        {
            Log::Error("AudioAsset '{}': could not create an OpenAL buffer.", name);
            file_.reset();
            return false;
        }

        // SoundBuffer::Load reports decode failures to stderr only; an empty AL buffer is how we notice.
        ALint size = 0, freq = 0, chans = 0, bits = 0;
        alGetBufferi(created->GetBuffer(), AL_SIZE, &size);
        alGetBufferi(created->GetBuffer(), AL_FREQUENCY, &freq);
        alGetBufferi(created->GetBuffer(), AL_CHANNELS, &chans);
        alGetBufferi(created->GetBuffer(), AL_BITS, &bits);
        alGetError();

        if (size <= 0 || freq <= 0 || chans <= 0 || bits <= 0)
        {
            Log::Error("AudioAsset '{}': decoded to zero samples (unsupported or corrupt file).", name);
            file_.reset();
            return false;
        }

        channels        = chans;
        sampleRate      = freq;
        durationSeconds = static_cast<float>(size) / static_cast<float>(chans * (bits / 8) * freq);
        buffer          = std::move(created);
        return true;
    }

    void AudioAsset::SerializeImportSettings(XMLNode &node) const
    {
        node.SetAttribute("defaultGain", settings.defaultGain);
        node.SetAttribute("defaultLoop", settings.defaultLoop);
        node.SetAttribute("busType", settings.busType);
    }

    void AudioAsset::DeserializeImportSettings(const XMLNode &node)
    {
        node.GetAttribute("defaultGain", settings.defaultGain);
        node.GetAttribute("defaultLoop", settings.defaultLoop);
        node.GetAttribute("busType", settings.busType);
    }
} // namespace SF::Engine
