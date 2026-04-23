#pragma once

#include <Resources/Resource.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "Audio.hpp"

namespace SF::Engine
{
    /**
     * @brief Factory base for registering sound buffer loaders by file extension.
     */
    template <typename Base>
    class SoundBufferFactory
    {
    public:
        using TLoadMethod = std::function<void(Base&, const std::filesystem::path&)>;
        using TWriteMethod = std::function<void(const Base&, const std::filesystem::path&)>;
        using TRegistryMap = std::unordered_map<std::string, std::pair<TLoadMethod, TWriteMethod>>;

        virtual ~SoundBufferFactory() = default;

        /**
         * @brief Gets the registry of file extension to loader/writer functions.
         */
        static TRegistryMap& Registry()
        {
            static TRegistryMap impl;
            return impl;
        }

        /**
         * @brief CRTP helper for registering derived loaders.
         */
        template <typename T>
        class Registrar
        {
        protected:
            /**
             * @brief Registers load/write methods for given file extensions.
             * @param names File extensions (e.g., ".wav", ".ogg").
             */
            template <typename... Args>
            static bool Register(Args&&... names)
            {
                for (std::string&& name : {names...})
                    SoundBufferFactory::Registry()[name] = std::make_pair(&T::Load, &T::Write);
                return true;
            }
        };
    };

    /**
     * @brief Resource representing an OpenAL sound buffer.
     *
     * Contains decoded audio data loaded from disk. Multiple AudioClip instances
     * can share the same SoundBuffer.
     */
    class SoundBuffer : public SoundBufferFactory<SoundBuffer>, public Resource
    {
    public:
        /**
         * @brief Creates or retrieves a cached sound buffer.
         * @param filename Path to the audio file.
         * @return Shared pointer to the sound buffer.
         */
        static std::shared_ptr<SoundBuffer> Create(const std::filesystem::path& filename);

        /**
         * @brief Constructs a sound buffer.
         * @param filename Path to the audio file.
         * @param load If true, loads the file immediately. Otherwise call Load() later.
         */
        explicit SoundBuffer(std::filesystem::path filename, bool load = true);
        ~SoundBuffer();

        // Delete copy operations (OpenAL buffer is non-copyable resource)
        SoundBuffer(const SoundBuffer&) = delete;
        SoundBuffer& operator=(const SoundBuffer&) = delete;

        // Move operations
        SoundBuffer(SoundBuffer&& other) noexcept;
        SoundBuffer& operator=(SoundBuffer&& other) noexcept;

        std::type_index GetTypeIndex() const override
        {
            return typeid(SoundBuffer);
        }

        const std::filesystem::path& GetFilename() const
        {
            return filename;
        }
        uint32_t GetBuffer() const
        {
            return buffer;
        }
        void SetBuffer(uint32_t buffer);

        /**
         * @brief Loads audio data from file using registered loader.
         */
        void Load();

    private:
        std::filesystem::path filename;
        uint32_t buffer = 0;

        // Cache for shared resources
        static std::unordered_map<std::string, std::weak_ptr<SoundBuffer>> cache;
        static std::mutex cacheMutex;
    };
}