#include "AtomicFSOperations.hpp"
#include <stdexcept>

namespace SF::Engine
{
    std::mutex AtomicFSOperations::s_registryMutex;
    std::unordered_map<std::string, std::shared_mutex> AtomicFSOperations::s_pathMutexes;

    std::shared_mutex &AtomicFSOperations::MutexForPath(const std::filesystem::path &path)
    {
        // Canonicalise so "foo/../bar" and "bar" map to the same mutex.
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec).string();
        if (ec)
            canonical = path.string(); // fallback if path doesn't exist yet

        std::lock_guard lock(s_registryMutex);
        return s_pathMutexes[canonical]; // default-constructs if missing
    }

    bool AtomicFSOperations::AtomicWriteFile(const std::filesystem::path &path,
                                             const std::vector<uint8_t> &data)
    {
        std::unique_lock lock(MutexForPath(path));

        auto temp = std::filesystem::path(path) += ".tmp";

        File f(temp.string());
        if (!f.Open(FileMode::Write))
            throw std::runtime_error("Cannot open temp file: " + temp.string());

        f.WriteAllBytes(data);
        f.Close();

        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            File::Delete(temp.string());
            return false;
        }
        return true;
    }

    bool AtomicFSOperations::AtomicWriteFile(const std::filesystem::path &path,
                                             const std::string &data)
    {
        return AtomicWriteFile(path, std::vector<uint8_t>(data.begin(), data.end()));
    }

    bool AtomicFSOperations::AtomicWriteFile(const std::filesystem::path &path,
                                             const char *data)
    {
        return AtomicWriteFile(path, std::string(data));
    }

    std::vector<uint8_t> AtomicFSOperations::AtomicReadFileBytes(const std::filesystem::path &path)
    {
        std::shared_lock lock(MutexForPath(path));

        File f(path.string());
        if (!f.Open(FileMode::Read))
            throw std::runtime_error("Cannot open file: " + path.string());

        return f.ReadAllBytes();
    }

    std::string AtomicFSOperations::AtomicReadFileText(const std::filesystem::path &path)
    {
        std::shared_lock lock(MutexForPath(path)); // shared

        File f(path.string());
        if (!f.Open(FileMode::Read))
            throw std::runtime_error("Cannot open file: " + path.string());

        return f.ReadAllText();
    }

} // namespace SF::Engine