#pragma once

#include "../EBML/Identifier.hpp"

namespace SF::Matroska
{
    // Matroska element IDs (from the Matroska specification)
    namespace ids
    {
        // Top-level elements
        inline constexpr SF::EBML::Identifier Segment{0x18538067};
        inline constexpr SF::EBML::Identifier SeekHead{0x114D9B74};
        inline constexpr SF::EBML::Identifier Info{0x1549A966};
        inline constexpr SF::EBML::Identifier Tracks{0x1654AE6B};
        inline constexpr SF::EBML::Identifier Cluster{0x1F43B675};
        inline constexpr SF::EBML::Identifier Cues{0x1C53BB6B};
        inline constexpr SF::EBML::Identifier Attachments{0x1941A469};
        inline constexpr SF::EBML::Identifier Chapters{0x1043A770};
        inline constexpr SF::EBML::Identifier Tags{0x1254C367};

        // Info elements
        inline constexpr SF::EBML::Identifier TimecodeScale{0x2AD7B1};
        inline constexpr SF::EBML::Identifier Duration{0x4489};
        inline constexpr SF::EBML::Identifier MuxingApp{0x4D80};
        inline constexpr SF::EBML::Identifier WritingApp{0x5741};
        inline constexpr SF::EBML::Identifier DateUTC{0x4461};
        inline constexpr SF::EBML::Identifier SegmentUID{0x73A4};
        inline constexpr SF::EBML::Identifier SegmentFilename{0x7384};
        inline constexpr SF::EBML::Identifier PrevUID{0x73CB};
        inline constexpr SF::EBML::Identifier PrevFilename{0x7383};
        inline constexpr SF::EBML::Identifier NextUID{0x73EB};
        inline constexpr SF::EBML::Identifier NextFilename{0x7382};
        inline constexpr SF::EBML::Identifier SegmentFamily{0x4444};
        inline constexpr SF::EBML::Identifier ChapterTranslate{0x6924};
        inline constexpr SF::EBML::Identifier TimecodeScaleDenominator{0x2AD7B3};

        // Track elements
        inline constexpr SF::EBML::Identifier TrackEntry{0xAE};
        inline constexpr SF::EBML::Identifier TrackNumber{0xD7};
        inline constexpr SF::EBML::Identifier TrackUID{0x73C5};
        inline constexpr SF::EBML::Identifier TrackType{0x83};
        inline constexpr SF::EBML::Identifier TrackEnabled{0xB9};
        inline constexpr SF::EBML::Identifier TrackDefault{0xBA};
        inline constexpr SF::EBML::Identifier TrackForced{0x55AA};
        inline constexpr SF::EBML::Identifier TrackLacing{0x9C};
        inline constexpr SF::EBML::Identifier MinCache{0x6DE7};
        inline constexpr SF::EBML::Identifier MaxCache{0x6DF8};
        inline constexpr SF::EBML::Identifier DefaultDuration{0x23E383};
        inline constexpr SF::EBML::Identifier DefaultDecodedFieldDuration{0x234E7A};
        inline constexpr SF::EBML::Identifier TrackTimecodeScale{0x23314F};
        inline constexpr SF::EBML::Identifier TrackOffset{0x537F};
        inline constexpr SF::EBML::Identifier MaxBlockAdditionID{0x55EE};
        inline constexpr SF::EBML::Identifier Name{0x536E};
        inline constexpr SF::EBML::Identifier Language{0x22B59C};
        inline constexpr SF::EBML::Identifier CodecID{0x86};
        inline constexpr SF::EBML::Identifier CodecPrivate{0x63A2};
        inline constexpr SF::EBML::Identifier CodecName{0x258688};
        inline constexpr SF::EBML::Identifier CodecDecodeAll{0xAA};
        inline constexpr SF::EBML::Identifier TrackOverlay{0x6FAB};
        inline constexpr SF::EBML::Identifier CodecDelay{0x56AA};
        inline constexpr SF::EBML::Identifier SeekPreRoll{0x56BB};

        // Video elements
        inline constexpr SF::EBML::Identifier Video{0xE0};
        inline constexpr SF::EBML::Identifier FlagInterlaced{0x9A};
        inline constexpr SF::EBML::Identifier FieldOrder{0x9D};
        inline constexpr SF::EBML::Identifier StereoMode{0x53B8};
        inline constexpr SF::EBML::Identifier AlphaMode{0x53C0};
        inline constexpr SF::EBML::Identifier OldStereoMode{0x53B9};
        inline constexpr SF::EBML::Identifier PixelWidth{0xB0};
        inline constexpr SF::EBML::Identifier PixelHeight{0xBA};
        inline constexpr SF::EBML::Identifier PixelCropBottom{0x54AA};
        inline constexpr SF::EBML::Identifier PixelCropTop{0x54BB};
        inline constexpr SF::EBML::Identifier PixelCropLeft{0x54CC};
        inline constexpr SF::EBML::Identifier PixelCropRight{0x54DD};
        inline constexpr SF::EBML::Identifier DisplayWidth{0x54B0};
        inline constexpr SF::EBML::Identifier DisplayHeight{0x54BA};
        inline constexpr SF::EBML::Identifier DisplayUnit{0x54B2};
        inline constexpr SF::EBML::Identifier AspectRatioType{0x54B3};
        inline constexpr SF::EBML::Identifier ColourSpace{0x2EB524};
        inline constexpr SF::EBML::Identifier GammaValue{0x2FB523};
        inline constexpr SF::EBML::Identifier FrameRate{0x2383E3};

        // Audio elements
        inline constexpr SF::EBML::Identifier Audio{0xE1};
        inline constexpr SF::EBML::Identifier SamplingFrequency{0xB5};
        inline constexpr SF::EBML::Identifier OutputSamplingFrequency{0x78B5};
        inline constexpr SF::EBML::Identifier Channels{0x9F};
        inline constexpr SF::EBML::Identifier BitDepth{0x6264};

        // Cluster elements
        inline constexpr SF::EBML::Identifier Timecode{0xE7};
        inline constexpr SF::EBML::Identifier SilentTracks{0x5854};
        inline constexpr SF::EBML::Identifier SilentTrackNumber{0x58D7};
        inline constexpr SF::EBML::Identifier Position{0xA7};
        inline constexpr SF::EBML::Identifier PrevSize{0xAB};
        inline constexpr SF::EBML::Identifier BlockGroup{0xA0};
        inline constexpr SF::EBML::Identifier Block{0xA1};
        inline constexpr SF::EBML::Identifier BlockVirtual{0xA2};
        inline constexpr SF::EBML::Identifier SimpleBlock{0xA3};
        inline constexpr SF::EBML::Identifier BlockAdditions{0x75A1};
        inline constexpr SF::EBML::Identifier BlockMore{0xA6};
        inline constexpr SF::EBML::Identifier BlockAddID{0xEE};
        inline constexpr SF::EBML::Identifier BlockAdditional{0xA5};
        inline constexpr SF::EBML::Identifier BlockDuration{0x9B};
        inline constexpr SF::EBML::Identifier ReferenceBlock{0xFB};
        inline constexpr SF::EBML::Identifier ReferenceVirtual{0xFD};
        inline constexpr SF::EBML::Identifier CodecState{0xA4};
        inline constexpr SF::EBML::Identifier DiscardPadding{0x75A2};
        inline constexpr SF::EBML::Identifier Slices{0x85};
        inline constexpr SF::EBML::Identifier TimeSlice{0x8E};
        inline constexpr SF::EBML::Identifier LaceNumber{0xCC};

        // Cues elements
        inline constexpr SF::EBML::Identifier CuePoint{0xBB};
        inline constexpr SF::EBML::Identifier CueTime{0xB3};
        inline constexpr SF::EBML::Identifier CueTrackPositions{0xB7};
        inline constexpr SF::EBML::Identifier CueTrack{0xF7};
        inline constexpr SF::EBML::Identifier CueClusterPosition{0xF1};
        inline constexpr SF::EBML::Identifier CueRelativePosition{0xF0};
        inline constexpr SF::EBML::Identifier CueDuration{0xB2};
        inline constexpr SF::EBML::Identifier CueBlockNumber{0x5378};
        inline constexpr SF::EBML::Identifier CueCodecState{0xEA};
        inline constexpr SF::EBML::Identifier CueReference{0xDB};
        inline constexpr SF::EBML::Identifier CueReferenceTime{0x96};

        // SeekHead elements
        inline constexpr SF::EBML::Identifier Seek{0x4DBB};
        inline constexpr SF::EBML::Identifier SeekID{0x53AB};
        inline constexpr SF::EBML::Identifier SeekPosition{0x53AC};

        // Attachments
        inline constexpr SF::EBML::Identifier AttachedFile{0x61A7};
        inline constexpr SF::EBML::Identifier FileDescription{0x467E};
        inline constexpr SF::EBML::Identifier FileName{0x466E};
        inline constexpr SF::EBML::Identifier FileMediaType{0x4660};
        inline constexpr SF::EBML::Identifier FileData{0x465C};
        inline constexpr SF::EBML::Identifier FileUID{0x46AE};

        // Chapters
        inline constexpr SF::EBML::Identifier EditionEntry{0x45B9};
        inline constexpr SF::EBML::Identifier ChapterAtom{0xB6};
        inline constexpr SF::EBML::Identifier ChapterUID{0x73C4};
        inline constexpr SF::EBML::Identifier ChapterTimeStart{0x91};
        inline constexpr SF::EBML::Identifier ChapterTimeEnd{0x92};
        inline constexpr SF::EBML::Identifier ChapterDisplay{0x80};
        inline constexpr SF::EBML::Identifier ChapString{0x85};
        inline constexpr SF::EBML::Identifier ChapLanguage{0x437C};

        // Tags
        inline constexpr SF::EBML::Identifier Tag{0x7373};
        inline constexpr SF::EBML::Identifier Targets{0x63C0};
        inline constexpr SF::EBML::Identifier TargetTypeValue{0x68CA};
        inline constexpr SF::EBML::Identifier SimpleTag{0x67C8};
        inline constexpr SF::EBML::Identifier TagName{0x45A3};
        inline constexpr SF::EBML::Identifier TagString{0x4487};

        // Content encoding (encryption/compression)
        inline constexpr SF::EBML::Identifier ContentEncodings{0x6D80};
        inline constexpr SF::EBML::Identifier ContentEncoding{0x6240};
        inline constexpr SF::EBML::Identifier ContentEncodingOrder{0x5031};
        inline constexpr SF::EBML::Identifier ContentEncodingScope{0x5032};
        inline constexpr SF::EBML::Identifier ContentEncodingType{0x5033};
        inline constexpr SF::EBML::Identifier ContentEncryption{0x5035};

        // Video colour/projection metadata (HDR, 360 video)
        inline constexpr SF::EBML::Identifier Colour{0x55B0};
        inline constexpr SF::EBML::Identifier MatrixCoefficients{0x55B1};
        inline constexpr SF::EBML::Identifier BitsPerChannel{0x55B2};
        inline constexpr SF::EBML::Identifier Projection{0x7670};
        inline constexpr SF::EBML::Identifier ProjectionType{0x7671};
    }
}