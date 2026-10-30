#include "VideoBuffer.hpp"

#include <dav1d/common.h>
#include <dav1d/dav1d.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace SF::Engine
{
    namespace fs = filesystem;

    unordered_map<string, weak_ptr<VideoBuffer>> VideoBuffer::cache;
    mutex VideoBuffer::cacheMutex;

    static fs::path PathOf(const File &f) { return f.GetFullPath(); }

    static string LowerExt(const fs::path &p)
    {
        string e = p.extension().string();
        ranges::transform(e, e.begin(), [](const unsigned char c) { return tolower(c); });
        return e;
    }

    shared_ptr<VideoBuffer> VideoBuffer::Create(const File &input)
    {
        const string key = fs::weakly_canonical(PathOf(input)).string();

        lock_guard lock(cacheMutex);
        if (const auto it = cache.find(key); it != cache.end())
            if (auto sp = it->second.lock())
                return sp;

        auto buffer = make_shared<VideoBuffer>(input, true);
        cache[key]  = buffer;
        return buffer;
    }

    VideoBuffer::VideoBuffer(File input, const bool load) : source(std::move(input))
    {
        if (load)
            Load();
    }

    VideoBuffer::~VideoBuffer() { CloseDecoder(); }

    VideoBuffer::VideoBuffer(VideoBuffer &&other) noexcept :
        source(std::move(other.source)), info(std::move(other.info)), packets(std::move(other.packets)),
        loaded(other.loaded), decoder(other.decoder), settings(other.settings), nextPacket(other.nextPacket),
        draining(other.draining), pending(other.pending)
    {
        other.decoder = nullptr;
        other.pending = {};
        other.loaded  = false;
    }

    VideoBuffer &VideoBuffer::operator=(VideoBuffer &&other) noexcept
    {
        if (this == &other)
            return *this;
        CloseDecoder();

        source     = std::move(other.source);
        info       = std::move(other.info);
        packets    = std::move(other.packets);
        loaded     = other.loaded;
        decoder    = other.decoder;
        settings   = other.settings;
        nextPacket = other.nextPacket;
        draining   = other.draining;
        pending    = other.pending;

        other.decoder = nullptr;
        other.pending = {};
        other.loaded  = false;
        return *this;
    }

    File VideoBuffer::ResolveToAv1Mkv(const File &src) const
    {
        const fs::path path = PathOf(src);
        const auto ext      = LowerExt(path);

        const auto it = Registry().find(ext);
        if (it == Registry().end())
            throw runtime_error("VideoBuffer: no loader registered for '" + ext + "'. Convert it to AV1 MKV first.");

        const auto &importFn = get<2>(it->second);
        if (!importFn)
            return src; // already a native (AV1 MKV) format

        // Transcode once, reuse next time while the source is unchanged.
        fs::path out = path;
        out.replace_extension(".av1.mkv"); // TODO: replace imported media with mkv :) :) trolling
        error_code ec;
        if (const bool fresh = fs::exists(out, ec) && fs::last_write_time(out, ec) >= fs::last_write_time(path, ec);
            !fresh)
            importFn(File(src), File(out));

        return {out};
    }

    void VideoBuffer::Load()
    {
        CloseDecoder();
        packets.clear();
        loaded = false;

        const File mkv = ResolveToAv1Mkv(source);

        const auto it = Registry().find(".mkv");
        if (it == Registry().end() || !get<0>(it->second))
            throw runtime_error("VideoBuffer: MKV loader not registered (include MkvVideoBuffer.hpp)");

        get<0>(it->second)(*this, File(mkv));
        loaded = true;
    }

    void VideoBuffer::Write(const File &output) const
    {
        const auto ext = LowerExt(PathOf(output));
        const auto it  = Registry().find(ext);
        if (it == Registry().end() || !get<1>(it->second))
            throw runtime_error("VideoBuffer: no writer registered for '" + ext + "'");
        get<1>(it->second)(*this, output);
    }

    void VideoBuffer::OpenDecoder()
    {
        dav1d_default_settings(&settings);
        settings.n_threads       = 0; // auto
        settings.max_frame_delay = 1; // low latency for realtime playback
        settings.all_layers      = 0;

        if (dav1d_open(&decoder, &settings) < 0)
        {
            decoder = nullptr;
            throw runtime_error("VideoBuffer: dav1d_open failed");
        }
        nextPacket = 0;
        draining   = false;
    }

    void VideoBuffer::CloseDecoder()
    {
        if (pending.sz > 0)
            dav1d_data_unref(&pending);
        pending = {};

        if (decoder)
            dav1d_close(&decoder); // also nulls the pointer
        decoder    = nullptr;
        nextPacket = 0;
        draining   = false;
    }

    bool VideoBuffer::DecodeNext(Dav1dPicture &out)
    {
        if (!loaded)
            return false;
        if (!decoder)
            OpenDecoder();

        for (;;)
        {
            int r = 0;
            if (pending.sz > 0)
            {
                r = dav1d_send_data(decoder, &pending);
                if (r < 0 && r != DAV1D_ERR(EAGAIN))
                {
                    dav1d_data_unref(&pending);
                    return false;
                }
            }

            r = dav1d_get_picture(decoder, &out);
            if (r == 0)
                return true;
            if (r != DAV1D_ERR(EAGAIN))
                return false;

            if (pending.sz == 0)
            {
                if (nextPacket < packets.size())
                {
                    const Av1Packet &p = packets[nextPacket++];
                    uint8_t *buf       = dav1d_data_create(&pending, p.Data.size());
                    if (!buf)
                        return false;
                    memcpy(buf, p.Data.data(), p.Data.size());
                    pending.m.timestamp = p.Pts;
                } else if (!draining)
                {
                    draining = true; // dav1d flushes remaining frames on the next get_picture
                } else
                {
                    return false; // end of stream
                }
            }
        }
    }

    void VideoBuffer::Seek(size_t frameIndex)
    {
        if (!decoder || packets.empty())
            return;

        frameIndex = min(frameIndex, packets.size() - 1);
        while (frameIndex > 0 && !packets[frameIndex].KeyFrame)
            --frameIndex;

        if (pending.sz > 0)
            dav1d_data_unref(&pending);
        pending = {};

        dav1d_flush(decoder);
        nextPacket = frameIndex;
        draining   = false;
        // NOTE: frames between the keyframe and the requested frame must be decoded and dropped by the caller.
    }
} // namespace SF::Engine
