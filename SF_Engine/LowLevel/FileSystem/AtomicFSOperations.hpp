#pragma once

#include <LowLevel/FileSystem/File.hpp>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace SF::Engine
{
    class AtomicFSOperations
    {
    public:
        static bool AtomicWriteFile(const std::filesystem::path &path, const std::vector<uint8_t> &data);
        static bool AtomicWriteFile(const std::filesystem::path &path, const std::string &data);
        static bool AtomicWriteFile(const std::filesystem::path &path, const char *data);

        static std::vector<uint8_t> AtomicReadFileBytes(const std::filesystem::path &path);
        static std::string AtomicReadFileText(const std::filesystem::path &path);

    private:
        // One shared_mutex per canonical path.
        // shared_lock  -> multiple concurrent readers.
        // unique_lock  -> one writer, blocks all readers.
        static std::shared_mutex &MutexForPath(const std::filesystem::path &path);

        static std::mutex s_registryMutex;
        static std::unordered_map<std::string, std::shared_mutex> s_pathMutexes;
    };
}