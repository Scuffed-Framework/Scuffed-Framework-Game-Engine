#pragma once

#include <Engine/Module.hpp>
#include <Rendering/RenderSystem.hpp>

#include <volk.h>
#include <vulkan/vulkan_video.hpp>

#include <array>
#include <cstdint>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
namespace SF::Engine
{
    using namespace std;

    enum class VideoCodec : uint8_t
    {
        H264,
        H265,
        AV1,
        // VP9
    };

    struct VideoCodecProfile
    {
        VideoCodec Codec{};
        uint32_t StdProfileIdc = 0;
        VkVideoChromaSubsamplingFlagBitsKHR Chroma{};
        VkVideoComponentBitDepthFlagBitsKHR BitDepth{};

        // pNext is always nullptr here; codec-specific values are flattened below
        VkVideoCapabilitiesKHR Capabilities{};
        VkVideoDecodeCapabilityFlagsKHR DecodeFlags = 0;
        uint32_t MaxLevel                           = 0; // H264/H265: maxLevelIdc, AV1: maxLevel
    };

    // if Vulkan can't convert our hated video formats to our own EBML stuff, use native system libraries, since like
    // who isn't using Windows, MacOS, or Linux for game dev?
    class Video : public ModuleRegistrar<Video>
    {
        friend class ModuleRegistrar<Video>;
        REGISTER_MODULE(Video, Module::Stage::Pre);

        vector<VideoCodecProfile> SupportedDecodeProfiles;

    public:
        Video()           = default;
        ~Video() override = default;

        bool Initialize() override
        {
            SupportedDecodeProfiles.clear();

            auto *renderSystem = RenderSystem::Get();
            if (!renderSystem)
                return InitFailure;

            auto *physicalDevice = renderSystem->GetPhysicalDevice();
            if (!physicalDevice || !*physicalDevice)
                return InitFailure;

            if (!vkGetPhysicalDeviceVideoCapabilitiesKHR)
                return InitFailure;

            // TODO: verify device extensions + a VK_QUEUE_VIDEO_DECODE_BIT_KHR queue family
            //       (VkQueueFamilyVideoPropertiesKHR::videoCodecOperations) before querying.

            struct Candidate
            {
                VideoCodec Codec;
                VkVideoCodecOperationFlagBitsKHR Op;
                uint32_t Idc;
            };

            constexpr array candidates = {
                    Candidate{VideoCodec::H264, VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
                              STD_VIDEO_H264_PROFILE_IDC_HIGH},
                    Candidate{VideoCodec::H264, VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
                              STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE},
                    Candidate{VideoCodec::H265, VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
                              STD_VIDEO_H265_PROFILE_IDC_MAIN},
                    Candidate{VideoCodec::H265, VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
                              STD_VIDEO_H265_PROFILE_IDC_MAIN_10},
                    Candidate{VideoCodec::H265, VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
                              STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS},
                    Candidate{VideoCodec::AV1, VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN},
                    Candidate{VideoCodec::AV1, VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, STD_VIDEO_AV1_PROFILE_HIGH},
                    Candidate{VideoCodec::AV1, VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR,
                              STD_VIDEO_AV1_PROFILE_PROFESSIONAL}

            };

            constexpr array ChromaFormats = {VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
                                             VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR,
                                             VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR};

            constexpr array BitDepths = {VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
                                         VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
                                         VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR};

            for (const auto &c: candidates)
            {
                for (const auto chroma: ChromaFormats)
                {
                    for (const auto depth: BitDepths)
                    {
                        // Codec-specific profile info (lives on the stack for the duration of the query)
                        VkVideoDecodeH264ProfileInfoKHR h264Profile{
                                .sType         = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,
                                .stdProfileIdc = static_cast<StdVideoH264ProfileIdc>(c.Idc),
                                .pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR};
                        VkVideoDecodeH265ProfileInfoKHR h265Profile{
                                .sType         = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,
                                .stdProfileIdc = static_cast<StdVideoH265ProfileIdc>(c.Idc)};
                        VkVideoDecodeAV1ProfileInfoKHR av1Profile{
                                .sType            = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR,
                                .stdProfile       = static_cast<StdVideoAV1Profile>(c.Idc),
                                .filmGrainSupport = VK_FALSE};

                        VkVideoProfileInfoKHR profile{.sType               = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
                                                      .videoCodecOperation = c.Op,
                                                      .chromaSubsampling   = chroma,
                                                      .lumaBitDepth        = depth,
                                                      .chromaBitDepth      = depth};

                        // Capability output chain
                        VkVideoDecodeH264CapabilitiesKHR h264Caps{
                                .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR};
                        VkVideoDecodeH265CapabilitiesKHR h265Caps{
                                .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR};
                        VkVideoDecodeAV1CapabilitiesKHR av1Caps{
                                .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR};

                        switch (c.Codec)
                        {
                            case VideoCodec::H264:
                                profile.pNext = &h264Profile;
                                break;
                            case VideoCodec::H265:
                                profile.pNext = &h265Profile;
                                break;
                            case VideoCodec::AV1:
                                profile.pNext = &av1Profile;
                                break;
                        }

                        void *codecCaps = c.Codec == VideoCodec::H264   ? static_cast<void *>(&h264Caps)
                                          : c.Codec == VideoCodec::H265 ? static_cast<void *>(&h265Caps)
                                                                        : static_cast<void *>(&av1Caps);

                        VkVideoDecodeCapabilitiesKHR decodeCaps{
                                .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR, .pNext = codecCaps};
                        VkVideoCapabilitiesKHR caps{.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR,
                                                    .pNext = &decodeCaps};

                        // Unsupported combos return VK_ERROR_VIDEO_PROFILE_*_NOT_SUPPORTED_KHR; expected, just skip.
                        if (vkGetPhysicalDeviceVideoCapabilitiesKHR(*physicalDevice, &profile, &caps) != VK_SUCCESS)
                            continue;

                        VideoCodecProfile result{};
                        result.Codec              = c.Codec;
                        result.StdProfileIdc      = c.Idc;
                        result.Chroma             = chroma;
                        result.BitDepth           = depth;
                        result.Capabilities       = caps;
                        result.Capabilities.pNext = nullptr; // don't keep dangling pointers
                        result.DecodeFlags        = decodeCaps.flags;
                        result.MaxLevel           = c.Codec == VideoCodec::H264   ? h264Caps.maxLevelIdc
                                                    : c.Codec == VideoCodec::H265 ? h265Caps.maxLevelIdc
                                                                                  : av1Caps.maxLevel;

                        SupportedDecodeProfiles.push_back(result);
                    }
                }
            }

            return InitSuccess;
        }

        void Update() override {}

        [[nodiscard]]
        const vector<VideoCodecProfile> &GetSupportedDecodeProfiles() const noexcept
        {
            return SupportedDecodeProfiles;
        }
    };
} // namespace SF::Engine

#pragma GCC diagnostic pop
