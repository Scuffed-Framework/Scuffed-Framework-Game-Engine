#pragma once

#include <Assets/AssetTraits.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include "Audio.hpp"

namespace SF::Engine
{
    // TODO: replace all write methods with some sort of EBML write. loading audio types is for editor
    /**
     * @brief Factory base for registering sound buffer loaders by file extension.
     */
    template<typename Base>
    class SoundBufferFactory
    {
    public:
        using TLoadMethod  = std::function<void(Base &, const DataInput &)>;
        using TWriteMethod = std::function<void(const Base &, const std::filesystem::path &)>;
        using TRegistryMap = std::unordered_map<std::string, std::pair<TLoadMethod, TWriteMethod>>;

        virtual ~SoundBufferFactory() = default;

        /**
         * @brief Gets the registry of file extension to loader/writer functions.
         */
        static TRegistryMap &Registry()
        {
            static TRegistryMap impl;
            return impl;
        }

        /**
         * @brief CRTP helper for registering derived loaders.
         */
        template<typename T>
        class Registrar
        {
        protected:
            /**
             * @brief Registers load/write methods for given file extensions.
             * @param names File extensions (e.g., ".wav", ".ogg").
             */
            template<typename... Args>
            static bool Register(Args &&...names)
            {
                TWriteMethod write; // empty = this format can't be written
                if constexpr (HasWriteMember<T>::value)
                {
                    // Catches a typo'd signature loudly instead of silently dropping the writer.
                    static_assert(
                            HasValidWrite<T, Base>::value,
                            "T::Write exists but doesn't match void(const SoundBuffer&, const std::filesystem::path&)");
                    write = &T::Write;
                }

                ((SoundBufferFactory::Registry()[std::string(names)] = std::make_pair(TLoadMethod(&T::Load), write)),
                 ...);
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
    // TODO: we are going to pull the same move in video, convert everything to mka, add PCM
    class SoundBuffer : public SoundBufferFactory<SoundBuffer>
    {
    public:
        /**
         * @breif 16 bit decoded PCM
         */
        struct PCMData
        {
            std::vector<int16_t> Samples;
            uint32_t SampleRate  = 0;
            uint8_t ChannelCount = 0;

            [[nodiscard]] bool Empty() const { return Samples.empty(); }
            [[nodiscard]] size_t FrameCount() const { return ChannelCount ? Samples.size() / ChannelCount : 0; }
            [[nodiscard]] float DurationSeconds() const
            {
                return SampleRate ? static_cast<float>(FrameCount()) / static_cast<float>(SampleRate) : 0.f;
            }
        };

        /**
         * @brief Creates or retrieves a cached sound buffer.
         * @param input Path to the audio file.
         * @return Shared pointer to the sound buffer.
         */
        static std::shared_ptr<SoundBuffer> Create(const DataInput &input);

        /**
         * @brief Wraps an already-existing OpenAL buffer handle.
         * @param existingBuffer A valid OpenAL buffer name; ownership transfers to the SoundBuffer.
         * @param input data
         */
        static std::shared_ptr<SoundBuffer> CreateFromHandle(ALuint existingBuffer, DataInput input = {});

        /**
         * @brief Creates a buffer filled with a procedurally generated waveform.
         */
        static std::shared_ptr<SoundBuffer> CreateWave(ALuint waveType, float frequency = 440.0f,
                                                       float durationSeconds = 1.0f, uint32_t sampleRate = 44100);

        /**
         * @brief Constructs a sound buffer.
         * @param input Path to the audio file.
         * @param load If true, loads the file immediately. Otherwise call Load() later.
         */
        explicit SoundBuffer(DataInput input, bool load = true);
        ~SoundBuffer();

        // Delete copy operations (OpenAL buffer is non-copyable resource)
        SoundBuffer(const SoundBuffer &)            = delete;
        SoundBuffer &operator=(const SoundBuffer &) = delete;

        // Move operations
        SoundBuffer(SoundBuffer &&other) noexcept;
        SoundBuffer &operator=(SoundBuffer &&other) noexcept;

        [[nodiscard]] std::type_index GetTypeIndex() const { return typeid(SoundBuffer); }

        [[nodiscard]] const DataInput &GetInput() const { return input; }
        [[nodiscard]] uint32_t GetBuffer() const { return buffer; }
        void SetBuffer(uint32_t buffer);

        /**
         * @brief Loads audio data from file using registered loader.
         */
        void Load();

        bool SetPCM(PCMData data);

        [[nodiscard]] const PCMData &GetPCM() const { return pcm; }
        [[nodiscard]] bool HasPCM() const { return !pcm.Empty(); }

        /** Frees the CPU-side copy (the AL buffer stays valid). Use in shipped builds. */
        void ReleasePCM() { PCMData().Samples.swap(pcm.Samples); }


    private:
        DataInput input;
        uint32_t buffer = 0;
        PCMData pcm;

        static std::unordered_map<DataInput, std::weak_ptr<SoundBuffer>> cache;
        static std::mutex cacheMutex;
    };
} // namespace SF::Engine
