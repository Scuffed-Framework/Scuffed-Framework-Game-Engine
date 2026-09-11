#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    // Bundle format constants
    static constexpr uint32_t BUNDLE_MAGIC = 0x53484452;  // 'SHDR'
    static constexpr uint32_t BUNDLE_VERSION = 1;

    struct ShaderBundleEntry
    {
        uint32_t nameHash;    // Hash of shader name for fast lookup
        uint32_t stage;       // VkShaderStageFlagBits
        uint32_t offset;      // Offset in SPIR-V data blob
        uint32_t size;        // Size in bytes
        char name[256];       // Human-readable name
        char entryPoint[64];  // Entry point function
    };

    struct ShaderBundleHeader
    {
        uint32_t magic;
        uint32_t version;
        uint32_t entryCount;
        uint32_t dataSize;
        uint32_t reserved[4];
    };

    class ShaderBundle
    {
    public:
        ShaderBundle() = default;

        // Add shader to bundle
        void addShader(const std::string& name, VkShaderStageFlagBits stage,
                       const std::vector<uint32_t>& spirv, const std::string& entryPoint = "main");

        // Get shader SPIR-V by name
        std::optional<std::vector<uint32_t>> getShader(const std::string& name,
                                                       VkShaderStageFlagBits stage) const;

        // Get shader by hash (faster)
        std::optional<std::vector<uint32_t>> getShaderByHash(uint32_t nameHash,
                                                             VkShaderStageFlagBits stage) const;

        // Save/Load bundle to/from file
        bool save(const std::string& filepath) const;
        bool load(const std::string& filepath);

        // Query
        size_t getShaderCount() const
        {
            return entries_.size();
        }
        std::vector<std::string> getShaderNames() const;
        bool hasShader(const std::string& name, VkShaderStageFlagBits stage) const;

        // Clear
        void clear();

    private:
        struct Entry
        {
            std::string name;
            std::string entryPoint;
            VkShaderStageFlagBits stage;
            uint32_t offset;
            uint32_t size;
            uint32_t nameHash;
        };

        std::vector<char> data_;                            // Concatenated SPIR-V data
        std::vector<Entry> entries_;                        // Shader entries
        std::unordered_map<uint32_t, size_t> hashToIndex_;  // Hash -> entry index

        static uint32_t hashString(const std::string& str);
        static uint32_t makeKey(uint32_t nameHash, VkShaderStageFlagBits stage);
    };

    // Implementation
    inline void ShaderBundle::addShader(const std::string& name, VkShaderStageFlagBits stage,
                                        const std::vector<uint32_t>& spirv,
                                        const std::string& entryPoint)
    {
        Entry entry;
        entry.name = name;
        entry.entryPoint = entryPoint;
        entry.stage = stage;
        entry.offset = static_cast<uint32_t>(data_.size());
        entry.size = static_cast<uint32_t>(spirv.size() * sizeof(uint32_t));
        entry.nameHash = hashString(name);

        // Append SPIR-V data
        const char* spirvData = reinterpret_cast<const char*>(spirv.data());
        data_.insert(data_.end(), spirvData, spirvData + entry.size);

        // Store entry
        uint32_t key = makeKey(entry.nameHash, stage);
        hashToIndex_[key] = entries_.size();
        entries_.push_back(entry);
    }

    inline std::optional<std::vector<uint32_t>> ShaderBundle::getShader(
        const std::string& name, VkShaderStageFlagBits stage) const
    {
        return getShaderByHash(hashString(name), stage);
    }

    inline std::optional<std::vector<uint32_t>> ShaderBundle::getShaderByHash(
        uint32_t nameHash, VkShaderStageFlagBits stage) const
    {
        uint32_t key = makeKey(nameHash, stage);
        auto it = hashToIndex_.find(key);

        if (it == hashToIndex_.end())
        {
            return std::nullopt;
        }

        const Entry& entry = entries_[it->second];

        std::vector<uint32_t> spirv(entry.size / sizeof(uint32_t));
        std::memcpy(spirv.data(), data_.data() + entry.offset, entry.size);

        return spirv;
    }

    inline bool ShaderBundle::save(const std::string& filepath) const
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        // Write header
        ShaderBundleHeader header;
        header.magic = BUNDLE_MAGIC;
        header.version = BUNDLE_VERSION;
        header.entryCount = static_cast<uint32_t>(entries_.size());
        header.dataSize = static_cast<uint32_t>(data_.size());

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Write entries
        for (const auto& entry : entries_)
        {
            ShaderBundleEntry diskEntry = {};
            diskEntry.nameHash = entry.nameHash;
            diskEntry.stage = static_cast<uint32_t>(entry.stage);
            diskEntry.offset = entry.offset;
            diskEntry.size = entry.size;

            strncpy(diskEntry.name, entry.name.c_str(), sizeof(diskEntry.name) - 1);
            strncpy(diskEntry.entryPoint, entry.entryPoint.c_str(),
                    sizeof(diskEntry.entryPoint) - 1);

            file.write(reinterpret_cast<const char*>(&diskEntry), sizeof(diskEntry));
        }

        // Write SPIR-V data
        file.write(data_.data(), data_.size());

        return true;
    }

    inline bool ShaderBundle::load(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        // Read header
        ShaderBundleHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != BUNDLE_MAGIC || header.version != BUNDLE_VERSION)
        {
            return false;
        }

        // Read entries
        entries_.clear();
        entries_.reserve(header.entryCount);
        hashToIndex_.clear();

        for (uint32_t i = 0; i < header.entryCount; i++)
        {
            ShaderBundleEntry diskEntry;
            file.read(reinterpret_cast<char*>(&diskEntry), sizeof(diskEntry));

            Entry entry;
            entry.name = diskEntry.name;
            entry.entryPoint = diskEntry.entryPoint;
            entry.stage = static_cast<VkShaderStageFlagBits>(diskEntry.stage);
            entry.offset = diskEntry.offset;
            entry.size = diskEntry.size;
            entry.nameHash = diskEntry.nameHash;

            uint32_t key = makeKey(entry.nameHash, entry.stage);
            hashToIndex_[key] = entries_.size();
            entries_.push_back(entry);
        }

        // Read SPIR-V data
        data_.resize(header.dataSize);
        file.read(data_.data(), header.dataSize);

        return true;
    }

    inline std::vector<std::string> ShaderBundle::getShaderNames() const
    {
        std::vector<std::string> names;
        names.reserve(entries_.size());
        for (const auto& entry : entries_)
        {
            names.push_back(entry.name);
        }
        return names;
    }

    inline bool ShaderBundle::hasShader(const std::string& name, VkShaderStageFlagBits stage) const
    {
        uint32_t key = makeKey(hashString(name), stage);
        return hashToIndex_.find(key) != hashToIndex_.end();
    }

    inline void ShaderBundle::clear()
    {
        entries_.clear();
        data_.clear();
        hashToIndex_.clear();
    }

    inline uint32_t ShaderBundle::hashString(const std::string& str)
    {
        // Simple FNV-1a hash
        uint32_t hash = 2166136261u;
        for (char c : str)
        {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    inline uint32_t ShaderBundle::makeKey(uint32_t nameHash, VkShaderStageFlagBits stage)
    {
        return nameHash ^ (static_cast<uint32_t>(stage) << 24);
    }
}