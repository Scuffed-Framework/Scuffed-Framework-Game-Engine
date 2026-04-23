#include "SoundBuffer.hpp"

#ifdef __APPLE__
#include <OpenAL/al.h>
#else
#include <al.h>
#endif
#include <Resources/Resources.hpp>
#include <iostream>
#include <stdexcept>

namespace SF::Engine
{
    // Initialize static members
    std::unordered_map<std::string, std::weak_ptr<SoundBuffer>> SoundBuffer::cache;
    std::mutex SoundBuffer::cacheMutex;

    std::shared_ptr<SoundBuffer> SoundBuffer::Create(const std::filesystem::path& filename)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        // Convert to absolute path for consistent caching
        std::string key = std::filesystem::absolute(filename).string();

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
        auto soundBuffer = std::make_shared<SoundBuffer>(filename, true);
        cache[key] = soundBuffer;

        return soundBuffer;
    }

    SoundBuffer::SoundBuffer(std::filesystem::path filename, bool load)
        : filename(std::move(filename)), buffer(0)
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

    SoundBuffer::SoundBuffer(SoundBuffer&& other) noexcept
        : filename(std::move(other.filename)), buffer(other.buffer)
    {
        other.buffer = 0;
    }

    SoundBuffer& SoundBuffer::operator=(SoundBuffer&& other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing buffer
            if (buffer != 0)
            {
                alDeleteBuffers(1, &buffer);
            }

            // Move data
            filename = std::move(other.filename);
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
        if (filename.empty())
        {
            std::cerr << "Cannot load sound buffer: filename is empty" << std::endl;
            return;
        }

        if (buffer == 0)
        {
            std::cerr << "Cannot load sound buffer: OpenAL buffer is invalid" << std::endl;
            return;
        }

        // Get file extension
        std::string extension = filename.extension().string();

        // Look up loader in registry
        auto& registry = Registry();
        auto it = registry.find(extension);

        if (it == registry.end())
        {
            std::cerr << "No loader registered for file extension: " << extension << std::endl;
            return;
        }

        // Call the registered loader
        try
        {
            it->second.first(*this, filename);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to load sound buffer from " << filename << ": " << e.what()
                      << std::endl;
        }
    }
}