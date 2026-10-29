#pragma once
#include <Assets/AssetTraits.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <UtilityClasses/StreamFactory.hpp>

#include <dav1d/dav1d.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    class VideoBuffer;

    /**
     * @brief: Raw (uncompressed) frame handed from a foreign demuxer/decoder to the AV1 encoder.
     * 8-bit or 10-bit planar 4:2:0 (10-bit = 16-bit little endian samples).
     */
    struct RawVideoFrame
    {
        uint32_t Width          = 0;
        uint32_t Height         = 0;
        uint8_t BitDepth        = 8;
        const uint8_t *Plane[3] = {};
        uint32_t Stride[3]      = {}; // bytes
        int64_t Pts             = 0;  // in frame units (0,1,2...) or timebase ticks
    };

    class IRawFrameSource
    {
    public:
        virtual ~IRawFrameSource()                     = default;
        [[nodiscard]] virtual uint32_t Width() const   = 0;
        [[nodiscard]] virtual uint32_t Height() const  = 0;
        [[nodiscard]] virtual uint32_t FpsNum() const  = 0;
        [[nodiscard]] virtual uint32_t FpsDen() const  = 0;
        [[nodiscard]] virtual uint8_t BitDepth() const = 0;
        /// Fill output with the next frame (pointers valid until the next call). false = end of stream.
        virtual bool Next(RawVideoFrame &out) = 0;
    };

    // One AV1 temporal unit (OBUs, as stored in a Matroska block).
    struct Av1Packet
    {
        vector<uint8_t> Data;
        int64_t Pts   = 0; // in `VideoInfo::TimebaseNum/Den` ticks
        bool KeyFrame = false;
    };

    struct VideoInfo
    {
        uint32_t Width       = 0;
        uint32_t Height      = 0;
        uint32_t FpsNum      = 0;
        uint32_t FpsDen      = 1;
        uint32_t TimebaseNum = 1;
        uint32_t TimebaseDen = 1000; // Matroska default: ms
        uint8_t BitDepth     = 8;
        vector<uint8_t> CodecPrivate; // av1C box payload (needed to write MKV)
    };

    template<typename Base>
    class VideoBufferFactory
    {
    public:
        /// Fills the buffer with AV1 packets. Only formats that already contain AV1 (MKV) implement real work here.
        using TLoadMethod = function<void(Base &, const File &)>;
        /// Writes the buffer's AV1 stream to a container.
        using TWriteMethod = function<void(const Base &, const File &)>;
        /// Foreign format -> AV1 MKV file on disk (transcode).
        using TImport      = function<void(const File &, const File &outMkv)>;
        using TRegistryMap = unordered_map<string, tuple<TLoadMethod, TWriteMethod, TImport>>;

        static TRegistryMap &Registry()
        {
            static TRegistryMap impl;
            return impl;
        }

        template<typename T>
        class Registrar
        {
        protected:
            // T provides (all optional except Write/Load for native formats):
            //   static void Load(Base&, const File&)          - native AV1 container
            //   static void Write(const Base&, const File&)
            //   static void Import(const File&, const File&)  - foreign -> AV1 MKV
            template<typename... Args>
            static bool Register(Args &&...names)
            {
                TLoadMethod load;
                TWriteMethod write;
                TImport imp;
                if constexpr (requires { &T::Load; })
                    load = &T::Load;
                if constexpr (HasWriteMember<T>::value)
                {
                    // Catches a typo'd signature loudly instead of silently dropping the writer.
                    static_assert(
                            HasValidWrite<T, Base>::value,
                            "T::Write exists but doesn't match void(const SoundBuffer&, const std::filesystem::path&)");
                    write = &T::Write;
                }
                if constexpr (requires { &T::Import; })
                    imp = &T::Import;

                ((VideoBufferFactory::Registry()[string(names)] = make_tuple(load, write, imp)), ...);
                return true;
            }
        };
    };

    /**
     * All VideoBuffers hold their data as AV1. Anything else is transcoded to an
     * AV1 MKV (cached next to the source / in the cache dir) and then loaded.
     */
    class VideoBuffer : public VideoBufferFactory<VideoBuffer>
    {
    public:
        static shared_ptr<VideoBuffer> Create(const File &input);

        explicit VideoBuffer(File input, bool load = true);
        ~VideoBuffer();

        VideoBuffer(const VideoBuffer &)            = delete;
        VideoBuffer &operator=(const VideoBuffer &) = delete;
        VideoBuffer(VideoBuffer &&other) noexcept;
        VideoBuffer &operator=(VideoBuffer &&other) noexcept;

        [[nodiscard]] type_index GetTypeIndex() const { return typeid(VideoBuffer); }

        // load mkv
        void Load();

        // write mkv
        void Write(const File &output) const;

        [[nodiscard]] const VideoInfo &Info() const { return info; }
        [[nodiscard]] const vector<Av1Packet> &Packets() const { return packets; }
        [[nodiscard]] size_t FrameCount() const { return packets.size(); }
        [[nodiscard]] bool IsLoaded() const { return loaded; }

        void SetInfo(VideoInfo i) { info = std::move(i); }
        void AddPacket(Av1Packet p) { packets.push_back(std::move(p)); }
        void MarkLoaded() { loaded = true; }

        bool DecodeNext(Dav1dPicture &out);
        void Seek(size_t frameIndex);

    private:
        void OpenDecoder();
        void CloseDecoder();
        File ResolveToAv1Mkv(const File &src) const;

        File source;
        VideoInfo info;
        vector<Av1Packet> packets;
        bool loaded = false;

        Dav1dContext *decoder  = nullptr;
        Dav1dSettings settings = {};
        size_t nextPacket      = 0;
        bool draining          = false;
        Dav1dData pending      = {};

        static unordered_map<string, weak_ptr<VideoBuffer>> cache;
        static mutex cacheMutex;
    };
} // namespace SF::Engine
