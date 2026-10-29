#pragma once
#include "../VideoBuffer.hpp"

#include <1stPartyLibs/Matroska/Demuxer.hpp>
#include <1stPartyLibs/Matroska/MatroskaFile.hpp>

#include <svt-av1/EbSvtAv1.h>
#include <svt-av1/EbSvtAv1Enc.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>

namespace SF::Engine
{
    namespace MkvDetail
    {
        inline std::unique_ptr<std::istream> OpenIn(const File &input)
        {
            return std::make_unique<std::ifstream>(input.GetFullPath(), std::ios::binary);
        }
        inline std::unique_ptr<std::ostream> OpenOut(const File &file)
        {
            return std::make_unique<std::ofstream>(file.GetFullPath(), std::ios::binary | std::ios::trunc);
        }

        inline constexpr const char *kAv1Codec = "V_AV1";
    } // namespace MkvDetail

    class MkvVideoBuffer : public VideoBuffer::Registrar<MkvVideoBuffer>
    {
        inline static const bool Registered = Register(".mkv");
        friend class VideoBuffer;

    public:
        static void Load(VideoBuffer &buffer, const File &input)
        {
            using namespace SF::Matroska;

            auto stream = MkvDetail::OpenIn(input);
            if (!stream || !*stream)
                throw std::runtime_error("MKV: cannot open input");

            Demuxer demux(*stream);
            if (!demux.initialize())
                throw std::runtime_error("MKV: not a Matroska stream");

            // First video track
            int videoIdx = -1;
            for (size_t i = 0; i < demux.track_count(); ++i)
                if (demux.track(static_cast<unsigned>(i)).type == TrackType::Video)
                {
                    videoIdx = static_cast<int>(i);
                    break;
                }
            if (videoIdx < 0)
                throw std::runtime_error("MKV: no video track");

            const Track &track = demux.track(static_cast<unsigned>(videoIdx));
            if (track.codecID != MkvDetail::kAv1Codec)
                throw std::runtime_error("MKV: video track is " + track.codecID +
                                         ", not AV1. Re-encode it (see Import hook).");

            const auto *video = std::get_if<VideoTrack>(&track.specific);
            if (!video)
                throw std::runtime_error("MKV: video track has no video data");

            VideoInfo info;
            info.Width        = static_cast<decltype(info.Width)>(video->pixelWidth);
            info.Height       = static_cast<decltype(info.Height)>(video->pixelHeight);
            info.CodecPrivate = std::vector<uint8_t>(video->codecPrivate.begin(), video->codecPrivate.end());

            // Frame rate: DefaultDuration (ns/frame) is exact, FrameRate is a fallback.
            if (track.defaultDuration.count() > 0)
            {
                const uint64_t g = std::gcd<uint64_t, uint64_t>(1000000000ull, track.defaultDuration.count());
                info.FpsNum      = static_cast<decltype(info.FpsNum)>(1000000000ull / g);
                info.FpsDen      = static_cast<decltype(info.FpsDen)>(track.defaultDuration.count() / g);
            } else if (video->frameRate > 0.0)
            {
                info.FpsNum = static_cast<decltype(info.FpsNum)>(video->frameRate * 1000.0 + 0.5);
                info.FpsDen = 1000;
            } else
            {
                info.FpsNum = 30;
                info.FpsDen = 1;
            }

            // Packet timestamps are in TimecodeScale ns ticks -> timebase = scale / 1e9
            {
                const uint64_t scale = demux.timecode_scale();
                const uint64_t g     = std::gcd<uint64_t, uint64_t>(scale, 1000000000ull);
                info.TimebaseNum     = static_cast<decltype(info.TimebaseNum)>(scale / g);
                info.TimebaseDen     = static_cast<decltype(info.TimebaseDen)>(1000000000ull / g);
            }

            // Bit depth from av1C byte 2: bit 6 = high_bitdepth, bit 5 = twelve_bit
            info.BitDepth = 8;
            if (video->codecPrivate.size() >= 3)
            {
                const auto flags = static_cast<uint8_t>(video->codecPrivate[2]);
                if (flags & 0x40)
                    info.BitDepth = (flags & 0x20) ? 12 : 10;
            }

            buffer.SetInfo(info);

            for (;;)
            {
                std::optional<Packet> pkt;
                const int r = demux.read_packet(pkt);
                if (r == 0)
                    break;
                if (r < 0)
                    throw std::runtime_error("MKV: demux error");
                if (pkt->track != static_cast<uint64_t>(videoIdx))
                    continue;

                Av1Packet p;
                for (const auto &f: pkt->frames) // video is never laced in practice; concat if it is
                    p.Data.insert(p.Data.end(), f.data.begin(), f.data.end());
                p.Pts = static_cast<decltype(p.Pts)>(pkt->timecode);
                // SimpleBlock -> flag is exact. BlockGroup -> keyframe iff it has no ReferenceBlock.
                p.KeyFrame = pkt->keyframe == KeyframeState::True ||
                             (pkt->keyframe == KeyframeState::Unknown && !pkt->hasReferenceBlock);
                buffer.AddPacket(std::move(p));
            }
        }

        static void Write(const VideoBuffer &buffer, const File &file)
        {
            using namespace SF::Matroska;

            const VideoInfo &info = buffer.Info();

            auto stream = MkvDetail::OpenOut(file);
            if (!stream || !*stream)
                throw std::runtime_error("MKV: cannot open output");

            MatroskaFile mkv; // declared after `stream` so it finalizes (destructor) before the stream closes

            // 1 tick = 1 timebase unit, so Pts are written verbatim and exactly.
            uint64_t scale = info.TimebaseDen ? (1000000000ull * info.TimebaseNum) / info.TimebaseDen : 0;
            if (scale == 0)
                scale = 1000000; // 1 ms fallback
            mkv.set_timecode_scale(scale);

            Track track;
            track.number  = 1;
            track.uid     = std::random_device{}() | 1ull; // must be non-zero
            track.type    = TrackType::Video;
            track.codecID = MkvDetail::kAv1Codec;
            track.lacing  = false;
            if (info.FpsNum)
                track.defaultDuration = std::chrono::nanoseconds(1000000000ull * info.FpsDen / info.FpsNum);

            VideoTrack vt;
            vt.codecID      = track.codecID;
            vt.pixelWidth   = info.Width;
            vt.pixelHeight  = info.Height;
            vt.codecPrivate = std::vector<EBML::byte>(info.CodecPrivate.size());
            std::memcpy(vt.codecPrivate.data(), info.CodecPrivate.data(), info.CodecPrivate.size());
            track.specific = std::move(vt);

            mkv.add_track(track);           // tracks must be added before...
            mkv.initialize_writer(*stream); // ...this writes EBML header, Segment, Info, Tracks

            bool open            = false;
            int64_t clusterStart = 0;

            for (const auto &[Data, Pts, KeyFrame]: buffer.Packets())
            {
                const auto pts = static_cast<int64_t>(Pts);
                if (constexpr int64_t kMaxClusterTicks = 30000;
                    !open || KeyFrame || pts - clusterStart >= kMaxClusterTicks)
                {
                    if (open)
                        mkv.end_cluster();
                    mkv.start_cluster(pts);
                    clusterStart = pts;
                    open         = true;
                }
                mkv.write_frame(
                        track.number, pts,
                        std::span<const EBML::byte>(reinterpret_cast<const EBML::byte *>(Data.data()), Data.size()),
                        KeyFrame);
            }

            if (open)
                mkv.end_cluster();
            mkv.finalize(); // Cues + trailing SeekHead, closes Segment
        }
        // No Import: .mkv is already the native format.
    };

    struct Av1EncodeSettings
    {
        int Preset        = 8;    // 0 (slow/best) .. 13 (fastest)
        int Crf           = 28;   // lower = better quality
        int KeyframeEvery = 120;  // frames; keep small so Seek() is cheap
        bool LowDelay     = true; // no frame reordering: friendlier for realtime playback
    };

    class Av1MkvEncoder
    {
    public:
        static void Encode(IRawFrameSource &src, const File &outMkv, const Av1EncodeSettings &s = {})
        {
            EbComponentType *handle = nullptr;
            EbSvtAv1EncConfiguration cfg{};
            if (svt_av1_enc_init_handle(&handle, &cfg) != EB_ErrorNone)
                throw std::runtime_error("SVT-AV1: init_handle failed");

            cfg.source_width           = src.Width();
            cfg.source_height          = src.Height();
            cfg.frame_rate_numerator   = src.FpsNum();
            cfg.frame_rate_denominator = src.FpsDen();
            cfg.encoder_bit_depth      = src.BitDepth() > 8 ? 10 : 8;
            cfg.encoder_color_format   = EB_YUV420;
            cfg.enc_mode               = static_cast<int8_t>(s.Preset);
            cfg.rate_control_mode      = 0; // CRF
            cfg.qp                     = s.Crf;
            cfg.intra_period_length    = s.KeyframeEvery - 1;
            cfg.pred_structure = static_cast<PredStructure>(s.LowDelay ? 1 : 2); // 1 = low delay, 2 = random access
            cfg.level_of_parallelism = 0;

            if (svt_av1_enc_set_parameter(handle, &cfg) != EB_ErrorNone || svt_av1_enc_init(handle) != EB_ErrorNone)
            {
                svt_av1_enc_deinit_handle(handle);
                throw std::runtime_error("SVT-AV1: parameter/init failed");
            }

            VideoInfo info;
            info.Width       = src.Width();
            info.Height      = src.Height();
            info.FpsNum      = src.FpsNum();
            info.FpsDen      = src.FpsDen();
            info.BitDepth    = static_cast<uint8_t>(cfg.encoder_bit_depth);
            info.TimebaseNum = src.FpsDen(); // 1 tick = 1 frame, exact for CFR sources
            info.TimebaseDen = src.FpsNum();

            EbBufferHeaderType *hdr = nullptr;
            if (svt_av1_enc_stream_header(handle, &hdr) == EB_ErrorNone && hdr)
            {
                // av1C: marker/version, profile/level, flags, reserved, then configOBUs (sequence header)
                const uint8_t highBitDepth = cfg.encoder_bit_depth > 8 ? 1 : 0;
                info.CodecPrivate = {0x81, 0x00 | 13, static_cast<uint8_t>(highBitDepth << 6 | 1 << 3 | 1 << 2), 0x00};
                info.CodecPrivate.insert(info.CodecPrivate.end(), hdr->p_buffer, hdr->p_buffer + hdr->n_filled_len);
                svt_av1_enc_stream_header_release(hdr);
            }

            // Encode into a VideoBuffer, then reuse the single MKV write path.
            VideoBuffer out(outMkv);
            out.SetInfo(info);

            auto drain = [&](bool final)
            {
                for (;;)
                {
                    EbBufferHeaderType *pkt = nullptr;
                    EbErrorType e           = svt_av1_enc_get_packet(handle, &pkt, final ? 1 : 0);
                    if (e == EB_NoErrorEmptyQueue || !pkt)
                        return false;

                    if (pkt->n_filled_len > 0)
                    {
                        Av1Packet p;
                        p.Data.assign(pkt->p_buffer, pkt->p_buffer + pkt->n_filled_len);
                        p.Pts      = pkt->pts;
                        p.KeyFrame = pkt->pic_type == EB_AV1_KEY_PICTURE;
                        out.AddPacket(std::move(p));
                    }
                    const bool eos = (pkt->flags & EB_BUFFERFLAG_EOS) != 0;
                    svt_av1_enc_release_out_buffer(&pkt);
                    if (eos)
                        return true;
                }
            };

            const uint32_t bytesPerSample = cfg.encoder_bit_depth > 8 ? 2 : 1;
            RawVideoFrame frame;
            int64_t index = 0;
            while (src.Next(frame))
            {
                EbSvtIOFormat io{};
                io.luma      = const_cast<uint8_t *>(frame.Plane[0]);
                io.cb        = const_cast<uint8_t *>(frame.Plane[1]);
                io.cr        = const_cast<uint8_t *>(frame.Plane[2]);
                io.y_stride  = frame.Stride[0] / bytesPerSample; // SVT wants strides in SAMPLES
                io.cb_stride = frame.Stride[1] / bytesPerSample;
                io.cr_stride = frame.Stride[2] / bytesPerSample;

                EbBufferHeaderType in{};
                in.size         = sizeof(in);
                in.p_buffer     = reinterpret_cast<uint8_t *>(&io);
                in.n_filled_len = frame.Stride[0] * frame.Height * 3 / 2; // informational
                in.pts          = index++;
                in.pic_type     = EB_AV1_INVALID_PICTURE;
                svt_av1_enc_send_picture(handle, &in);

                drain(false); // non-blocking: collect whatever is ready
            }

            EbBufferHeaderType eos{};
            eos.size  = sizeof(eos);
            eos.flags = EB_BUFFERFLAG_EOS;
            svt_av1_enc_send_picture(handle, &eos);
            drain(true); // blocking until the EOS packet

            svt_av1_enc_deinit(handle);
            svt_av1_enc_deinit_handle(handle);

            MkvVideoBuffer::Write(out, outMkv);
        }
    };
} // namespace SF::Engine
