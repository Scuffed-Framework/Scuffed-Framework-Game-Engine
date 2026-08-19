#include "SoundBuffer.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif
#include <iostream>
#include <stdexcept>
#include "Waves.hpp"

namespace SF::Engine
{
    // Initialize static members
    std::unordered_map<DataInput, std::weak_ptr<SoundBuffer>> SoundBuffer::cache;
    std::mutex SoundBuffer::cacheMutex;

    std::shared_ptr<SoundBuffer> SoundBuffer::Create(const DataInput &input)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        DataInput key;
        // Convert to absolute path for consistent caching
        if (input.memory == nullptr)
            key.file = input.file;
        else
            key.memory = (input.memory);

        // Check if already cached
        auto it = cache.find(key);
        if (it != cache.end())
        {
            if (auto existing = it->second.lock())
            {
                return existing;
            }
            // Expired weak_ptr, remove it
            cache.erase(it);
        }

        // Create new sound buffer
        auto soundBuffer = std::make_shared<SoundBuffer>(input, true);
        cache[key] = soundBuffer;

        return soundBuffer;
    }

    SoundBuffer::SoundBuffer(DataInput input, bool load)
        : input(std::move(input)), buffer(0)
    {
        // Generate OpenAL buffer
        alGenBuffers(1, &buffer);
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            std::cerr << "Failed to generate OpenAL buffer: " << error << std::endl;
            buffer = 0;
            return;
        }

        if (load)
        {
            Load();
        }
    }

    SoundBuffer::~SoundBuffer()
    {
        if (buffer != 0)
        {
            alDeleteBuffers(1, &buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "Failed to delete OpenAL buffer: " << error << std::endl;
            }
        }
    }

    SoundBuffer::SoundBuffer(SoundBuffer &&other) noexcept
        : input(std::move(other.input)), buffer(other.buffer)
    {
        other.buffer = 0;
    }

    SoundBuffer &SoundBuffer::operator=(SoundBuffer &&other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing buffer
            if (buffer != 0)
            {
                alDeleteBuffers(1, &buffer);
            }

            // Move data
            input = std::move(other.input);
            buffer = other.buffer;
            other.buffer = 0;
        }
        return *this;
    }

    void SoundBuffer::SetBuffer(uint32_t newBuffer)
    {
        // Delete old buffer if valid
        if (this->buffer != 0)
        {
            alDeleteBuffers(1, &this->buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "Failed to delete old OpenAL buffer: " << error << std::endl;
            }
        }

        this->buffer = newBuffer;
    }

    void SoundBuffer::Load()
    {
        if (!input.file->Exists() || input.memory == nullptr)
        {
            std::cerr << "Cannot load sound buffer: input is empty" << std::endl;
            return;
        }

        if (buffer == 0)
        {
            std::cerr << "Cannot load sound buffer: OpenAL buffer is invalid" << std::endl;
            return;
        }

        if (input.file != nullptr)
        {
            // Get file extension
            std::string extension = input.file->GetExtension();

            // Look up loader in registry
            auto &registry = Registry();
            auto it = registry.find(extension);

            if (it == registry.end())
            {
                std::cerr << "No loader registered for file extension: " << extension << std::endl;
                return;
            }
            // Call the registered loader
            try
            {
                it->second.first(*this, input);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to load sound buffer from " << input.file->GetFullPath() << input.memory << ": " << e.what()
                          << std::endl;
            }
        }
        else // reading from mem
        {
            alBufferData(this->GetBuffer(), AL_FORMAT_STEREO16, input.memory,
                         static_cast<ALsizei>(64),
                         static_cast<ALsizei>(128));

            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "Failed to buffer data from memory" << error << std::endl;
            }
        }
    }

    std::shared_ptr<SoundBuffer> SoundBuffer::CreateFromHandle(ALuint existingBuffer, DataInput input)
    {
        if (existingBuffer == 0 || alIsBuffer(existingBuffer) == AL_FALSE)
        {
            std::cerr << "CreateFromHandle: not a valid OpenAL buffer" << std::endl;
            return nullptr;
        }

        // Construct without generating/loading, then adopt the caller's handle.
        auto sb = std::make_shared<SoundBuffer>(std::move(input), /*load=*/false);
        sb->SetBuffer(existingBuffer);
        return sb;
    }

    std::shared_ptr<SoundBuffer> SoundBuffer::CreateWave(ALuint waveType, float frequency,
                                                         float durationSeconds, uint32_t sampleRate)
    {
        auto sb = std::make_shared<SoundBuffer>(DataInput{}, /*load=*/false);
        if (sb->buffer == 0)
            return nullptr;

        const size_t sampleCount = static_cast<size_t>(durationSeconds * sampleRate);
        std::vector<int16_t> samples(sampleCount);

        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);
        constexpr float PI = 3.14159265358979323846f;

        for (size_t i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float phase = t * frequency;
            float value = 0.0f;

            switch (waveType)
            {
            case ExtraAudioWaves::Wave_Sine:
                value = std::sin(2.0f * PI * phase);
                break;
            case ExtraAudioWaves::Wave_Square:
                value = std::fmod(phase, 1.0f) < 0.5f ? 1.0f : -1.0f;
                break;
            case ExtraAudioWaves::Wave_Triangle:
                value = 4.0f * std::fabs(std::fmod(phase + 0.75f, 1.0f) - 0.5f) - 1.0f;
                break;
            case ExtraAudioWaves::Wave_Sawtooth:
                value = 2.0f * std::fmod(phase, 1.0f) - 1.0f;
                break;
            case ExtraAudioWaves::Wave_WhiteNoise:
                value = noiseDist(rng);
                break;
            default:
                std::cerr << "CreateWave: unknown wave type " << waveType << std::endl;
                break;
            }

            samples[i] = static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
        }

        alBufferData(sb->buffer, AL_FORMAT_MONO16, samples.data(),
                     static_cast<ALsizei>(samples.size() * sizeof(int16_t)),
                     static_cast<ALsizei>(sampleRate));

        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
            std::cerr << "CreateWave: alBufferData failed: " << error << std::endl;

        return sb;
    }
}