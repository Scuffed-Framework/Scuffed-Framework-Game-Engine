#pragma once

#include "../EBML/Schema.hpp"
#include "MatroskaIds.hpp"

namespace SF::Matroska
{
    inline EBML::Schema create_matroska_schema()
    {
        using namespace EBML;
        
        Schema schema;

        // --- EBML header (RFC 8794) ---
        // Every well-formed EBML/Matroska file starts with one of these; a
        // schema that can't classify them can still parse a file (unknown
        // IDs default to Binary) but reports wrong kinds for it.
        schema.set(EBML::ids::EBML, Element::Kind::Master);
        schema.set(EBML::ids::EBMLVersion, Element::Kind::UInt);
        schema.set(EBML::ids::EBMLReadVersion, Element::Kind::UInt);
        schema.set(EBML::ids::EBMLMaxIDLength, Element::Kind::UInt);
        schema.set(EBML::ids::EBMLMaxSizeLength, Element::Kind::UInt);
        schema.set(EBML::ids::DocType, Element::Kind::String);
        schema.set(EBML::ids::DocTypeVersion, Element::Kind::UInt);
        schema.set(EBML::ids::DocTypeReadVersion, Element::Kind::UInt);
        schema.set(EBML::ids::Void, Element::Kind::Binary);
        schema.set(EBML::ids::CRC32, Element::Kind::Binary);
        schema.add_children(EBML::ids::EBML, {
            EBML::ids::EBMLVersion, EBML::ids::EBMLReadVersion,
            EBML::ids::EBMLMaxIDLength, EBML::ids::EBMLMaxSizeLength,
            EBML::ids::DocType, EBML::ids::DocTypeVersion,
            EBML::ids::DocTypeReadVersion
        });

        // Register all element kinds
        // Master elements
        schema.set(ids::Segment, Element::Kind::Master);
        schema.set(ids::SeekHead, Element::Kind::Master);
        schema.set(ids::Info, Element::Kind::Master);
        schema.set(ids::Tracks, Element::Kind::Master);
        schema.set(ids::Cluster, Element::Kind::Master);
        schema.set(ids::Cues, Element::Kind::Master);
        schema.set(ids::Attachments, Element::Kind::Master);
        schema.set(ids::Chapters, Element::Kind::Master);
        schema.set(ids::Tags, Element::Kind::Master);
        schema.set(ids::TrackEntry, Element::Kind::Master);
        schema.set(ids::Video, Element::Kind::Master);
        schema.set(ids::Audio, Element::Kind::Master);
        schema.set(ids::BlockGroup, Element::Kind::Master);
        schema.set(ids::BlockAdditions, Element::Kind::Master);
        schema.set(ids::BlockMore, Element::Kind::Master);
        schema.set(ids::Slices, Element::Kind::Master);
        schema.set(ids::TimeSlice, Element::Kind::Master);
        schema.set(ids::CuePoint, Element::Kind::Master);
        schema.set(ids::CueTrackPositions, Element::Kind::Master);
        schema.set(ids::Seek, Element::Kind::Master);
        schema.set(ids::SilentTracks, Element::Kind::Master);
        schema.set(ids::ChapterTranslate, Element::Kind::Master);
        schema.set(ids::AttachedFile, Element::Kind::Master);
        schema.set(ids::EditionEntry, Element::Kind::Master);
        schema.set(ids::ChapterAtom, Element::Kind::Master);
        schema.set(ids::ChapterDisplay, Element::Kind::Master);
        schema.set(ids::Tag, Element::Kind::Master);
        schema.set(ids::Targets, Element::Kind::Master);
        schema.set(ids::SimpleTag, Element::Kind::Master);
        schema.set(ids::ContentEncodings, Element::Kind::Master);
        schema.set(ids::ContentEncoding, Element::Kind::Master);
        schema.set(ids::ContentEncryption, Element::Kind::Master);
        schema.set(ids::Colour, Element::Kind::Master);
        schema.set(ids::Projection, Element::Kind::Master);

        // UInt elements
        schema.set(ids::TimecodeScale, Element::Kind::UInt);
        schema.set(ids::Duration, Element::Kind::Float);
        schema.set(ids::DateUTC, Element::Kind::Date);
        schema.set(ids::SegmentUID, Element::Kind::Binary);
        schema.set(ids::SegmentFilename, Element::Kind::String);
        schema.set(ids::PrevUID, Element::Kind::Binary);
        schema.set(ids::NextUID, Element::Kind::Binary);
        schema.set(ids::TrackNumber, Element::Kind::UInt);
        schema.set(ids::TrackUID, Element::Kind::UInt);
        schema.set(ids::TrackType, Element::Kind::UInt);
        schema.set(ids::TrackEnabled, Element::Kind::UInt);
        schema.set(ids::TrackDefault, Element::Kind::UInt);
        schema.set(ids::TrackForced, Element::Kind::UInt);
        schema.set(ids::TrackLacing, Element::Kind::UInt);
        schema.set(ids::MinCache, Element::Kind::UInt);
        schema.set(ids::MaxCache, Element::Kind::UInt);
        schema.set(ids::DefaultDuration, Element::Kind::UInt);
        schema.set(ids::DefaultDecodedFieldDuration, Element::Kind::UInt);
        schema.set(ids::TrackTimecodeScale, Element::Kind::Float);
        schema.set(ids::TrackOffset, Element::Kind::UInt);
        schema.set(ids::MaxBlockAdditionID, Element::Kind::UInt);
        schema.set(ids::Language, Element::Kind::String);
        schema.set(ids::CodecID, Element::Kind::String);
        schema.set(ids::CodecPrivate, Element::Kind::Binary);
        schema.set(ids::CodecName, Element::Kind::String);
        schema.set(ids::CodecDecodeAll, Element::Kind::UInt);
        schema.set(ids::TrackOverlay, Element::Kind::UInt);
        schema.set(ids::CodecDelay, Element::Kind::UInt);
        schema.set(ids::SeekPreRoll, Element::Kind::UInt);
        schema.set(ids::PixelWidth, Element::Kind::UInt);
        schema.set(ids::PixelHeight, Element::Kind::UInt);
        schema.set(ids::PixelCropBottom, Element::Kind::UInt);
        schema.set(ids::PixelCropTop, Element::Kind::UInt);
        schema.set(ids::PixelCropLeft, Element::Kind::UInt);
        schema.set(ids::PixelCropRight, Element::Kind::UInt);
        schema.set(ids::DisplayWidth, Element::Kind::UInt);
        schema.set(ids::DisplayHeight, Element::Kind::UInt);
        schema.set(ids::DisplayUnit, Element::Kind::UInt);
        schema.set(ids::AspectRatioType, Element::Kind::UInt);
        schema.set(ids::ColourSpace, Element::Kind::Binary);
        schema.set(ids::GammaValue, Element::Kind::Float);
        schema.set(ids::FrameRate, Element::Kind::Float);
        schema.set(ids::SamplingFrequency, Element::Kind::Float);
        schema.set(ids::OutputSamplingFrequency, Element::Kind::Float);
        schema.set(ids::Channels, Element::Kind::UInt);
        schema.set(ids::BitDepth, Element::Kind::UInt);
        schema.set(ids::Timecode, Element::Kind::UInt);
        schema.set(ids::Position, Element::Kind::UInt);
        schema.set(ids::PrevSize, Element::Kind::UInt);
        schema.set(ids::Block, Element::Kind::Binary);
        schema.set(ids::SimpleBlock, Element::Kind::Binary);
        schema.set(ids::BlockAddID, Element::Kind::UInt);
        schema.set(ids::BlockAdditional, Element::Kind::Binary);
        schema.set(ids::BlockDuration, Element::Kind::UInt);
        schema.set(ids::ReferenceBlock, Element::Kind::Int);
        schema.set(ids::CodecState, Element::Kind::UInt);
        schema.set(ids::DiscardPadding, Element::Kind::Int);
        schema.set(ids::LaceNumber, Element::Kind::UInt);
        schema.set(ids::CueTime, Element::Kind::UInt);
        schema.set(ids::CueTrack, Element::Kind::UInt);
        schema.set(ids::CueClusterPosition, Element::Kind::UInt);
        schema.set(ids::CueRelativePosition, Element::Kind::UInt);
        schema.set(ids::CueDuration, Element::Kind::UInt);
        schema.set(ids::CueBlockNumber, Element::Kind::UInt);
        schema.set(ids::CueCodecState, Element::Kind::UInt);
        schema.set(ids::CueReference, Element::Kind::UInt);
        schema.set(ids::CueReferenceTime, Element::Kind::UInt);
        schema.set(ids::SeekID, Element::Kind::Binary);
        schema.set(ids::SeekPosition, Element::Kind::UInt);
        schema.set(ids::Name, Element::Kind::Utf8);
        schema.set(ids::MuxingApp, Element::Kind::Utf8);
        schema.set(ids::WritingApp, Element::Kind::Utf8);
        schema.set(ids::FlagInterlaced, Element::Kind::UInt);
        schema.set(ids::FieldOrder, Element::Kind::UInt);
        schema.set(ids::StereoMode, Element::Kind::UInt);
        schema.set(ids::AlphaMode, Element::Kind::UInt);
        schema.set(ids::OldStereoMode, Element::Kind::UInt);

        // Attachments
        schema.set(ids::FileDescription, Element::Kind::Utf8);
        schema.set(ids::FileName, Element::Kind::Utf8);
        schema.set(ids::FileMediaType, Element::Kind::String);
        schema.set(ids::FileData, Element::Kind::Binary);
        schema.set(ids::FileUID, Element::Kind::UInt);

        // Chapters
        schema.set(ids::ChapterUID, Element::Kind::UInt);
        schema.set(ids::ChapterTimeStart, Element::Kind::UInt);
        schema.set(ids::ChapterTimeEnd, Element::Kind::UInt);
        schema.set(ids::ChapString, Element::Kind::Utf8);
        schema.set(ids::ChapLanguage, Element::Kind::String);

        // Tags
        schema.set(ids::TargetTypeValue, Element::Kind::UInt);
        schema.set(ids::TagName, Element::Kind::Utf8);
        schema.set(ids::TagString, Element::Kind::Utf8);

        // Content encoding
        schema.set(ids::ContentEncodingOrder, Element::Kind::UInt);
        schema.set(ids::ContentEncodingScope, Element::Kind::UInt);
        schema.set(ids::ContentEncodingType, Element::Kind::UInt);

        // Colour / Projection
        schema.set(ids::MatrixCoefficients, Element::Kind::UInt);
        schema.set(ids::BitsPerChannel, Element::Kind::UInt);
        schema.set(ids::ProjectionType, Element::Kind::UInt);

        // Register parent-child relationships for unknown-size parsing.
        // Void and CRC32 are legal direct children of nearly any master
        // element in real-world files (muxers use them for padding/checksums
        // wherever convenient), so they're added everywhere below rather
        // than declared once.
        schema.add_children(ids::Segment, {
            ids::SeekHead, ids::Info, ids::Tracks, ids::Cluster, ids::Cues, ids::Attachments, ids::Chapters, ids::Tags,
            EBML::ids::Void, EBML::ids::CRC32
        });
        
        schema.add_children(ids::Cluster, {
            ids::Timecode, ids::SilentTracks, ids::Position, ids::PrevSize, ids::BlockGroup, ids::SimpleBlock,
            EBML::ids::Void, EBML::ids::CRC32
        });
        
        schema.add_children(ids::BlockGroup, {
            ids::Block, ids::BlockVirtual, ids::BlockAdditions, ids::BlockDuration, ids::ReferenceBlock,
            ids::ReferenceVirtual, ids::CodecState, ids::DiscardPadding, ids::Slices,
            EBML::ids::Void
        });
        
        schema.add_children(ids::Tracks, { ids::TrackEntry, EBML::ids::Void, EBML::ids::CRC32 });
        
        schema.add_children(ids::TrackEntry, {
            ids::TrackNumber, ids::TrackUID, ids::TrackType, ids::TrackEnabled, ids::TrackDefault,
            ids::TrackForced, ids::TrackLacing, ids::MinCache, ids::MaxCache, ids::DefaultDuration,
            ids::DefaultDecodedFieldDuration, ids::TrackTimecodeScale, ids::TrackOffset,
            ids::MaxBlockAdditionID, ids::Name, ids::Language, ids::CodecID, ids::CodecPrivate,
            ids::CodecName, ids::CodecDecodeAll, ids::TrackOverlay, ids::CodecDelay, ids::SeekPreRoll,
            ids::Video, ids::Audio, ids::ContentEncodings, EBML::ids::Void
        });
        
        schema.add_children(ids::Video, {
            ids::FlagInterlaced, ids::FieldOrder, ids::StereoMode, ids::AlphaMode, ids::OldStereoMode,
            ids::PixelWidth, ids::PixelHeight, ids::PixelCropBottom, ids::PixelCropTop,
            ids::PixelCropLeft, ids::PixelCropRight, ids::DisplayWidth, ids::DisplayHeight,
            ids::DisplayUnit, ids::AspectRatioType, ids::ColourSpace, ids::GammaValue, ids::FrameRate,
            ids::Colour, ids::Projection
        });
        
        schema.add_children(ids::Audio, {
            ids::SamplingFrequency, ids::OutputSamplingFrequency, ids::Channels, ids::BitDepth
        });

        schema.add_children(ids::Colour, { ids::MatrixCoefficients, ids::BitsPerChannel });
        schema.add_children(ids::Projection, { ids::ProjectionType });

        schema.add_children(ids::ContentEncodings, { ids::ContentEncoding });
        schema.add_children(ids::ContentEncoding, {
            ids::ContentEncodingOrder, ids::ContentEncodingScope, ids::ContentEncodingType, ids::ContentEncryption
        });
        
        schema.add_children(ids::Cues, { ids::CuePoint, EBML::ids::Void, EBML::ids::CRC32 });
        schema.add_children(ids::CuePoint, { ids::CueTime, ids::CueTrackPositions });
        schema.add_children(ids::CueTrackPositions, {
            ids::CueTrack, ids::CueClusterPosition, ids::CueRelativePosition, ids::CueDuration,
            ids::CueBlockNumber, ids::CueCodecState, ids::CueReference
        });
        
        schema.add_children(ids::SeekHead, { ids::Seek, EBML::ids::Void, EBML::ids::CRC32 });
        schema.add_children(ids::Seek, { ids::SeekID, ids::SeekPosition });
        schema.add_children(ids::Info, {
            ids::TimecodeScale, ids::Duration, ids::MuxingApp, ids::WritingApp, ids::DateUTC,
            ids::SegmentUID, ids::SegmentFilename, ids::PrevUID, ids::PrevFilename, ids::NextUID,
            ids::NextFilename, ids::SegmentFamily, ids::ChapterTranslate, ids::TimecodeScaleDenominator,
            EBML::ids::Void
        });

        schema.add_children(ids::Attachments, { ids::AttachedFile, EBML::ids::Void, EBML::ids::CRC32 });
        schema.add_children(ids::AttachedFile, {
            ids::FileDescription, ids::FileName, ids::FileMediaType, ids::FileData, ids::FileUID
        });

        schema.add_children(ids::Chapters, { ids::EditionEntry, EBML::ids::Void, EBML::ids::CRC32 });
        schema.add_children(ids::EditionEntry, { ids::ChapterAtom });
        schema.add_children(ids::ChapterAtom, {
            ids::ChapterUID, ids::ChapterTimeStart, ids::ChapterTimeEnd, ids::ChapterDisplay, ids::ChapterAtom
        });
        schema.add_children(ids::ChapterDisplay, { ids::ChapString, ids::ChapLanguage });

        schema.add_children(ids::Tags, { ids::Tag, EBML::ids::Void, EBML::ids::CRC32 });
        schema.add_children(ids::Tag, { ids::Targets, ids::SimpleTag });
        schema.add_children(ids::Targets, { ids::TargetTypeValue, ids::CueTrack });
        schema.add_children(ids::SimpleTag, { ids::TagName, ids::TagString, ids::SimpleTag });

        return schema;
    }
}