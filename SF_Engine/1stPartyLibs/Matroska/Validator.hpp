#pragma once

#include "MatroskaMuxer.hpp"
#include <vector>
#include <string>
#include <functional>
#include <unordered_set>

namespace SF::Matroska
{
    struct ValidationResult
    {
        bool valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        void add_error(const std::string& error)
        {
            valid = false;
            errors.push_back(error);
        }
        
        void add_warning(const std::string& warning)
        {
            warnings.push_back(warning);
        }
    };

    class MatroskaValidator
    {
    public:
        ValidationResult validate_tracks(const std::unordered_map<uint64_t, Track>& tracks)
        {
            ValidationResult result;
            
            if (tracks.empty())
            {
                result.add_error("No tracks defined");
                return result;
            }
            
            std::unordered_set<uint64_t> trackNumbers;
            for (const auto& [num, track] : tracks)
            {
                if (!trackNumbers.insert(num).second)
                    result.add_error("Duplicate track number: " + std::to_string(num));
                
                if (num == 0)
                    result.add_error("Track number cannot be 0");
                
                if (track.uid == 0)
                    result.add_warning("Track UID is 0 (should be unique)");
                
                if (track.codecID.empty())
                    result.add_error("Track codec ID is empty");
                
                if (track.type == TrackType::Video)
                {
                    if (auto* v = std::get_if<VideoTrack>(&track.specific))
                    {
                        if (v->pixelWidth == 0 || v->pixelHeight == 0)
                            result.add_error("Video track has invalid dimensions");
                        if (v->codecID.empty())
                            result.add_error("Video track has no codec ID");
                    }
                    else
                    {
                        result.add_error("Video track has no video specific data");
                    }
                }
                else if (track.type == TrackType::Audio)
                {
                    if (auto* a = std::get_if<AudioTrack>(&track.specific))
                    {
                        if (a->samplingFrequency <= 0)
                            result.add_error("Audio track has invalid sampling frequency");
                        if (a->channels == 0)
                            result.add_error("Audio track has 0 channels");
                        if (a->codecID.empty())
                            result.add_error("Audio track has no codec ID");
                    }
                    else
                    {
                        result.add_error("Audio track has no audio specific data");
                    }
                }
            }
            
            return result;
        }

        ValidationResult validate_frames(const std::vector<CuePoint>& cues, 
                                         uint64_t expectedFrames = 0)
        {
            ValidationResult result;
            
            if (cues.empty() && expectedFrames > 0)
            {
                result.add_warning("No cue points found (seeking will be slow)");
            }
            
            uint64_t lastTime = 0;
            for (const auto& cue : cues)
            {
                if (cue.timecode < lastTime)
                    result.add_warning("Cue times are not monotonically increasing");
                lastTime = cue.timecode;
                
                if (cue.positions.empty())
                    result.add_warning("Cue point has no positions");
            }
            
            return result;
        }

        // Fixed: using MatroskaMuxer::Cluster which is now public
        ValidationResult validate_clusters(const std::vector<MatroskaMuxer::Cluster>& clusters)
        {
            ValidationResult result;
            
            if (clusters.empty())
            {
                result.add_error("No clusters found");
                return result;
            }
            
            uint64_t lastTimecode = 0;
            for (const auto& cluster : clusters)
            {
                if (cluster.timecode < lastTimecode)
                    result.add_error("Cluster timecodes are not monotonically increasing");
                lastTimecode = cluster.timecode;
                
                if (cluster.frameCount == 0)
                    result.add_warning("Cluster has no frames");
                
                if (cluster.duration.count() <= 0)
                    result.add_warning("Cluster has zero duration");
            }
            
            return result;
        }

        ValidationResult validate_file(const std::string& filename)
        {
            ValidationResult result;
            
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                result.add_error("Cannot open file: " + filename);
                return result;
            }
            
            size_t fileSize = file.tellg();
            if (fileSize < 1024)
                result.add_error("File is too small to be valid EBML");
            
            file.seekg(0);
            std::vector<byte> header(20);
            file.read(reinterpret_cast<char*>(header.data()), header.size());
            
            if (header[0] != 0x1A || header[1] != 0x45 || 
                header[2] != 0xDF || header[3] != 0xA3)
            {
                result.add_error("File does not start with EBML header");
            }
            
            return result;
        }
    };
}