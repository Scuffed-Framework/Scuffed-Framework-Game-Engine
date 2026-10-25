#pragma once

#include "../EBML/Identifier.hpp"


// Matroska element IDs (from the Matroska specification)
namespace SF::Matroska::ids
{
    // Top-level elements
    inline constexpr Identifier Segment{0x18538067};
    inline constexpr Identifier SeekHead{0x114D9B74};
    inline constexpr Identifier Info{0x1549A966};
    inline constexpr Identifier Tracks{0x1654AE6B};
    inline constexpr Identifier Cluster{0x1F43B675};
    inline constexpr Identifier Cues{0x1C53BB6B};
    inline constexpr Identifier Attachments{0x1941A469};
    inline constexpr Identifier Chapters{0x1043A770};
    inline constexpr Identifier Tags{0x1254C367};

    // Info elements
    inline constexpr Identifier TimecodeScale{0x2AD7B1};
    inline constexpr Identifier Duration{0x4489};
    inline constexpr Identifier MuxingApp{0x4D80};
    inline constexpr Identifier WritingApp{0x5741};
    inline constexpr Identifier DateUTC{0x4461};
    inline constexpr Identifier SegmentUID{0x73A4};
    inline constexpr Identifier SegmentFilename{0x7384};
    inline constexpr Identifier PrevUID{0x73CB};
    inline constexpr Identifier PrevFilename{0x7383};
    inline constexpr Identifier NextUID{0x73EB};
    inline constexpr Identifier NextFilename{0x7382};
    inline constexpr Identifier SegmentFamily{0x4444};
    inline constexpr Identifier ChapterTranslate{0x6924};
    inline constexpr Identifier TimecodeScaleDenominator{0x2AD7B3};
    inline constexpr Identifier Title{0x7BA9};

    // Track elements
    inline constexpr Identifier TrackEntry{0xAE};
    inline constexpr Identifier TrackNumber{0xD7};
    inline constexpr Identifier TrackUID{0x73C5};
    inline constexpr Identifier TrackType{0x83};
    inline constexpr Identifier TrackEnabled{0xB9};
    inline constexpr Identifier TrackDefault{0xBA};
    inline constexpr Identifier TrackForced{0x55AA};
    inline constexpr Identifier TrackLacing{0x9C};
    inline constexpr Identifier MinCache{0x6DE7};
    inline constexpr Identifier MaxCache{0x6DF8};
    inline constexpr Identifier DefaultDuration{0x23E383};
    inline constexpr Identifier DefaultDecodedFieldDuration{0x234E7A};
    inline constexpr Identifier TrackTimecodeScale{0x23314F};
    inline constexpr Identifier TrackOffset{0x537F};
    inline constexpr Identifier MaxBlockAdditionID{0x55EE};
    inline constexpr Identifier Name{0x536E};
    inline constexpr Identifier Language{0x22B59C};
    inline constexpr Identifier CodecID{0x86};
    inline constexpr Identifier CodecPrivate{0x63A2};
    inline constexpr Identifier CodecName{0x258688};
    inline constexpr Identifier CodecDecodeAll{0xAA};
    inline constexpr Identifier TrackOverlay{0x6FAB};
    inline constexpr Identifier CodecDelay{0x56AA};
    inline constexpr Identifier SeekPreRoll{0x56BB};

    // Video elements
    inline constexpr Identifier Video{0xE0};
    inline constexpr Identifier FlagInterlaced{0x9A};
    inline constexpr Identifier FieldOrder{0x9D};
    inline constexpr Identifier StereoMode{0x53B8};
    inline constexpr Identifier AlphaMode{0x53C0};
    inline constexpr Identifier OldStereoMode{0x53B9};
    inline constexpr Identifier PixelWidth{0xB0};
    inline constexpr Identifier PixelHeight{0xBA};
    inline constexpr Identifier PixelCropBottom{0x54AA};
    inline constexpr Identifier PixelCropTop{0x54BB};
    inline constexpr Identifier PixelCropLeft{0x54CC};
    inline constexpr Identifier PixelCropRight{0x54DD};
    inline constexpr Identifier DisplayWidth{0x54B0};
    inline constexpr Identifier DisplayHeight{0x54BA};
    inline constexpr Identifier DisplayUnit{0x54B2};
    inline constexpr Identifier AspectRatioType{0x54B3};
    inline constexpr Identifier ColorSpace{0x2EB524};
    inline constexpr Identifier GammaValue{0x2FB523};
    inline constexpr Identifier FrameRate{0x2383E3};

    // Audio elements
    inline constexpr Identifier Audio{0xE1};
    inline constexpr Identifier SamplingFrequency{0xB5};
    inline constexpr Identifier OutputSamplingFrequency{0x78B5};
    inline constexpr Identifier Channels{0x9F};
    inline constexpr Identifier BitDepth{0x6264};

    // Cluster elements
    inline constexpr Identifier Timecode{0xE7};
    inline constexpr Identifier SilentTracks{0x5854};
    inline constexpr Identifier SilentTrackNumber{0x58D7};
    inline constexpr Identifier Position{0xA7};
    inline constexpr Identifier PrevSize{0xAB};
    inline constexpr Identifier BlockGroup{0xA0};
    inline constexpr Identifier Block{0xA1};
    inline constexpr Identifier BlockVirtual{0xA2};
    inline constexpr Identifier SimpleBlock{0xA3};
    inline constexpr Identifier BlockAdditions{0x75A1};
    inline constexpr Identifier BlockMore{0xA6};
    inline constexpr Identifier BlockAddID{0xEE};
    inline constexpr Identifier BlockAdditional{0xA5};
    inline constexpr Identifier BlockDuration{0x9B};
    inline constexpr Identifier ReferenceBlock{0xFB};
    inline constexpr Identifier ReferenceVirtual{0xFD};
    inline constexpr Identifier CodecState{0xA4};
    inline constexpr Identifier DiscardPadding{0x75A2};
    inline constexpr Identifier Slices{0x85};
    inline constexpr Identifier TimeSlice{0x8E};
    inline constexpr Identifier LaceNumber{0xCC};

    // Cues elements
    inline constexpr Identifier CuePoint{0xBB};
    inline constexpr Identifier CueTime{0xB3};
    inline constexpr Identifier CueTrackPositions{0xB7};
    inline constexpr Identifier CueTrack{0xF7};
    inline constexpr Identifier CueClusterPosition{0xF1};
    inline constexpr Identifier CueRelativePosition{0xF0};
    inline constexpr Identifier CueDuration{0xB2};
    inline constexpr Identifier CueBlockNumber{0x5378};
    inline constexpr Identifier CueCodecState{0xEA};
    inline constexpr Identifier CueReference{0xDB};
    inline constexpr Identifier CueReferenceTime{0x96};

    // SeekHead elements
    inline constexpr Identifier Seek{0x4DBB};
    inline constexpr Identifier SeekID{0x53AB};
    inline constexpr Identifier SeekPosition{0x53AC};

    // Attachments
    inline constexpr Identifier AttachedFile{0x61A7};
    inline constexpr Identifier FileDescription{0x467E};
    inline constexpr Identifier FileName{0x466E};
    inline constexpr Identifier FileMediaType{0x4660};
    inline constexpr Identifier FileData{0x465C};
    inline constexpr Identifier FileUID{0x46AE};

    // Chapters
    inline constexpr Identifier EditionEntry{0x45B9};
    inline constexpr Identifier ChapterAtom{0xB6};
    inline constexpr Identifier ChapterUID{0x73C4};
    inline constexpr Identifier ChapterTimeStart{0x91};
    inline constexpr Identifier ChapterTimeEnd{0x92};
    inline constexpr Identifier ChapterDisplay{0x80};
    inline constexpr Identifier ChapString{0x85};
    inline constexpr Identifier ChapLanguage{0x437C};

    // Tags
    inline constexpr Identifier Tag{0x7373};
    inline constexpr Identifier Targets{0x63C0};
    inline constexpr Identifier TargetTypeValue{0x68CA};
    inline constexpr Identifier SimpleTag{0x67C8};
    inline constexpr Identifier TagName{0x45A3};
    inline constexpr Identifier TagString{0x4487};

    // Content encoding (encryption/compression)
    inline constexpr Identifier ContentEncodings{0x6D80};
    inline constexpr Identifier ContentEncoding{0x6240};
    inline constexpr Identifier ContentEncodingOrder{0x5031};
    inline constexpr Identifier ContentEncodingScope{0x5032};
    inline constexpr Identifier ContentEncodingType{0x5033};
    inline constexpr Identifier ContentEncryption{0x5035};

    // Video colour/projection metadata (HDR, 360 video)
    inline constexpr Identifier Colour{0x55B0};
    inline constexpr Identifier MatrixCoefficients{0x55B1};
    inline constexpr Identifier BitsPerChannel{0x55B2};
    inline constexpr Identifier Projection{0x7670};
    inline constexpr Identifier ProjectionType{0x7671};
} // namespace SF::Matroska::ids
