#include "File.hpp"
#include <cstring>
#include <system_error>
#include <thread>

namespace SF::Engine
{

    // File class implementation
    File::File(const File &other)
        : m_path(other.m_path), m_cachedSize(other.m_cachedSize) {}

    File::File(File &&other) noexcept
        : m_path(std::move(other.m_path)), m_stream(std::move(other.m_stream)), m_cachedSize(other.m_cachedSize), m_isOpen(other.m_isOpen)
    {
        other.m_isOpen = false;
        other.m_cachedSize = 0;
    }

    File::~File()
    {
        Close();
    }

    File &File::operator=(const File &other)
    {
        if (this != &other)
        {
            Close();
            m_path = other.m_path;
            m_cachedSize = other.m_cachedSize;
        }
        return *this;
    }

    File &File::operator=(File &&other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_path = std::move(other.m_path);
            m_stream = std::move(other.m_stream);
            m_cachedSize = other.m_cachedSize;
            m_isOpen = other.m_isOpen;

            other.m_isOpen = false;
            other.m_cachedSize = 0;
        }
        return *this;
    }

    std::ios::openmode File::ConvertMode(FileMode mode) const
    {
        switch (mode)
        {
        case FileMode::Read:
            return std::ios::in | std::ios::binary;
        case FileMode::Write:
            return std::ios::out | std::ios::binary;
        case FileMode::Append:
            return std::ios::app | std::ios::binary;
        case FileMode::ReadWrite:
            return std::ios::in | std::ios::out | std::ios::binary;
        case FileMode::ReadAppend:
            return std::ios::in | std::ios::app | std::ios::binary;
        default:
            return std::ios::in | std::ios::binary;
        }
    }

    bool File::Open(FileMode mode)
    {
        if (m_isOpen)
        {
            Close();
        }

        m_stream.open(m_path, ConvertMode(mode));
        m_isOpen = m_stream.is_open();

        if (m_isOpen)
        {
            UpdateCache();
        }

        return m_isOpen;
    }

    void File::Close()
    {
        if (m_stream.is_open())
        {
            m_stream.close();
        }
        m_isOpen = false;
    }

    bool File::IsOpen() const
    {
        return m_isOpen;
    }

    std::string File::ReadAllText()
    {
        if (!m_isOpen && !Open(FileMode::Read))
        {
            return "";
        }

        m_stream.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(m_stream.tellg());
        m_stream.seekg(0, std::ios::beg);

        std::string text(size, '\0');
        m_stream.read(&text[0], static_cast<std::streamsize>(size));

        return text;
    }

    std::vector<std::string> File::ReadAllLines()
    {
        std::vector<std::string> lines;
        std::string line;

        if (!m_isOpen && !Open(FileMode::Read))
        {
            return lines;
        }

        m_stream.seekg(0, std::ios::beg);

        while (std::getline(m_stream, line))
        {
            lines.push_back(line);
        }

        return lines;
    }

    std::vector<uint8_t> File::ReadAllBytes()
    {
        if (!m_isOpen && !Open(FileMode::Read))
        {
            return {};
        }

        m_stream.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(m_stream.tellg());
        m_stream.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        m_stream.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

        return data;
    }

    size_t File::Read(void *buffer, size_t size)
    {
        if (!m_isOpen && !Open(FileMode::Read))
        {
            return 0;
        }

        m_stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
        return static_cast<size_t>(m_stream.gcount());
    }

    std::string File::ReadString(size_t length)
    {
        std::string str(length, '\0');
        size_t bytesRead = Read(&str[0], length);
        str.resize(bytesRead);
        return str;
    }

    int64_t File::ReadInt64()
    {
        int64_t value = 0;
        Read(&value, sizeof(int64_t));
        return value;
    }

    int32_t File::ReadInt32()
    {
        int32_t value = 0;
        Read(&value, sizeof(int32_t));
        return value;
    }

    double File::ReadDouble()
    {
        double value = 0;
        Read(&value, sizeof(double));
        return value;
    }

    float File::ReadFloat()
    {
        float value = 0;
        Read(&value, sizeof(float));
        return value;
    }

    size_t File::Write(const void *data, size_t size)
    {
        if (!m_isOpen && !Open(FileMode::Write))
        {
            return 0;
        }

        size_t pos = static_cast<size_t>(m_stream.tellp());
        m_stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));

        size_t newPos = static_cast<size_t>(m_stream.tellp());
        size_t bytesWritten = newPos - pos;

        if (newPos > m_cachedSize)
        {
            m_cachedSize = newPos;
        }

        return bytesWritten;
    }

    void File::WriteAllText(const std::string &text)
    {
        Write(text.data(), text.size());
    }

    void File::WriteAllBytes(const std::vector<uint8_t> &data)
    {
        Write(data.data(), data.size());
    }

    void File::WriteInt64(int64_t value)
    {
        Write(&value, sizeof(int64_t));
    }

    void File::WriteInt32(int32_t value)
    {
        Write(&value, sizeof(int32_t));
    }

    void File::WriteDouble(double value)
    {
        Write(&value, sizeof(double));
    }

    void File::WriteFloat(float value)
    {
        Write(&value, sizeof(float));
    }

    size_t File::GetPosition()
    {
        if (!m_isOpen)
            return 0;
        return static_cast<size_t>(m_stream.tellg());
    }

    bool File::SetPosition(size_t position, FileSeek seek)
    {
        if (!m_isOpen)
            return false;

        std::ios::seekdir dir;
        switch (seek)
        {
        case FileSeek::Begin:
            dir = std::ios::beg;
            break;
        case FileSeek::Current:
            dir = std::ios::cur;
            break;
        case FileSeek::End:
            dir = std::ios::end;
            break;
        default:
            dir = std::ios::beg;
        }

        m_stream.seekg(static_cast<std::streamoff>(position), dir);
        m_stream.seekp(static_cast<std::streamoff>(position), dir);

        return m_stream.good();
    }

    bool File::Seek(int64_t offset, FileSeek seek)
    {
        if (!m_isOpen)
            return false;

        std::ios::seekdir dir;
        switch (seek)
        {
        case FileSeek::Begin:
            dir = std::ios::beg;
            break;
        case FileSeek::Current:
            dir = std::ios::cur;
            break;
        case FileSeek::End:
            dir = std::ios::end;
            break;
        default:
            dir = std::ios::cur;
        }

        m_stream.seekg(static_cast<std::streamoff>(offset), dir);
        m_stream.seekp(static_cast<std::streamoff>(offset), dir);

        return m_stream.good();
    }

    bool File::Delete(const std::string &path)
    {
        std::error_code ec;
        return fs::remove(path, ec);
    }

    bool File::Delete()
    {
        Close();
        std::error_code ec;
        return fs::remove(m_path, ec);
    }

    bool File::Copy(const std::string &source, const std::string &destination, bool overwrite)
    {
        std::error_code ec;

        if (overwrite)
        {
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        }
        else
        {
            fs::copy_file(source, destination, ec);
        }

        return !ec;
    }

    bool File::CopyTo(const std::string &destination, bool overwrite) const
    {
        return Copy(m_path.string(), destination, overwrite);
    }

    bool File::Move(const std::string &source, const std::string &destination, bool overwrite)
    {
        if (overwrite && Exists(destination))
        {
            if (!Delete(destination))
                return false;
        }

        std::error_code ec;
        fs::rename(source, destination, ec);
        return !ec;
    }

    bool File::MoveTo(const std::string &destination, bool overwrite)
    {
        if (m_isOpen)
            Close();

        if (overwrite && Exists(destination))
        {
            if (!Delete(destination))
                return false;
        }

        std::error_code ec;
        fs::rename(m_path, destination, ec);
        if (!ec)
        {
            m_path = destination;
        }
        return !ec;
    }

    bool File::Rename(const std::string &oldPath, const std::string &newPath)
    {
        return Move(oldPath, newPath);
    }

    bool File::Rename(const std::string &newName)
    {
        fs::path newPath = m_path.parent_path() / newName;
        return MoveTo(newPath.string());
    }

    std::string File::GetNameWithoutExtension() const
    {
        return m_path.stem().string();
    }

    std::string File::GetDirectory() const
    {
        return m_path.parent_path().string();
    }

    FileType File::GetType() const
    {
        if (!fs::exists(m_path))
            return FileType::Unknown;

        if (fs::is_regular_file(m_path))
            return FileType::Regular;
        if (fs::is_directory(m_path))
            return FileType::Directory;
        if (fs::is_symlink(m_path))
            return FileType::SymbolicLink;

        return FileType::Unknown;
    }

    FileTime File::GetTimes() const
    {
        FileTime times;
        std::error_code ec;

        if (fs::exists(m_path))
        {
            // Note: C++17 filesystem doesn't have creation_time
            // Creation time is set to epoch (or same as last write time)
            auto ftime = fs::last_write_time(m_path, ec);
            if (!ec)
            {
                // Convert file_time_type to system_clock::time_point
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                times.lastWriteTime = sctp;
                times.creationTime = sctp;   // Fallback: use last write time
                times.lastAccessTime = sctp; // Fallback: use last write time
            }
        }

        return times;
    }

    bool File::IsSymbolicLink() const
    {
        return fs::is_symlink(m_path);
    }

    bool File::CreateDirectory(const std::string &path)
    {
        std::error_code ec;
        return fs::create_directories(path, ec);
    }

    bool File::DeleteDirectory(const std::string &path, bool recursive)
    {
        std::error_code ec;

        if (recursive)
        {
            return fs::remove_all(path, ec) > 0;
        }
        else
        {
            return fs::remove(path, ec);
        }
    }

    std::vector<File> File::GetFiles(const std::string &directory, const std::string &pattern, bool recursive)
    {
        std::vector<File> files;

        try
        {
            if (recursive)
            {
                for (const auto &entry : fs::recursive_directory_iterator(directory))
                {
                    if (entry.is_regular_file())
                    {
                        if (pattern == "*" || entry.path().extension() == pattern)
                        {
                            files.emplace_back(entry.path());
                        }
                    }
                }
            }
            else
            {
                for (const auto &entry : fs::directory_iterator(directory))
                {
                    if (entry.is_regular_file())
                    {
                        if (pattern == "*" || entry.path().extension() == pattern)
                        {
                            files.emplace_back(entry.path());
                        }
                    }
                }
            }
        }
        catch (const std::exception &)
        {
            // Directory might not exist or we don't have permissions
        }

        return files;
    }

    std::vector<File> File::GetDirectories(const std::string &directory, bool recursive)
    {
        std::vector<File> directories;

        try
        {
            if (recursive)
            {
                for (const auto &entry : fs::recursive_directory_iterator(directory))
                {
                    if (entry.is_directory())
                    {
                        directories.emplace_back(entry.path());
                    }
                }
            }
            else
            {
                for (const auto &entry : fs::directory_iterator(directory))
                {
                    if (entry.is_directory())
                    {
                        directories.emplace_back(entry.path());
                    }
                }
            }
        }
        catch (const std::exception &)
        {
            // Directory might not exist or we don't have permissions
        }

        return directories;
    }

    std::string File::GetCurrentDirectory()
    {
        return fs::current_path().string();
    }

    bool File::SetCurrentDirectory(const std::string &path)
    {
        std::error_code ec;
        fs::current_path(path, ec);
        return !ec;
    }

    std::string File::GetTempDirectory()
    {
        return fs::temp_directory_path().string();
    }

    File File::CreateTempFile()
    {
        std::string tempDir = GetTempDirectory();
        std::string tempName = "tmp_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        fs::path tempPath = fs::path(tempDir) / tempName;

        File tempFile(tempPath);
        tempFile.Open(FileMode::Write);
        return tempFile;
    }

    void File::UpdateCache()
    {
        if (fs::exists(m_path))
        {
            m_cachedSize = static_cast<size_t>(fs::file_size(m_path));
        }
        else
        {
            m_cachedSize = 0;
        }
    }

    // DirectoryIterator implementation
    File::DirectoryIterator::DirectoryIterator(const std::string &path)
    {
        try
        {
            m_iterator = fs::recursive_directory_iterator(path);
        }
        catch (const std::exception &)
        {
            m_iterator = fs::recursive_directory_iterator();
        }
    }

    File::DirectoryIterator File::DirectoryIterator::begin() const
    {
        return *this;
    }

    File::DirectoryIterator File::DirectoryIterator::end() const
    {
        return DirectoryIterator();
    }

    bool File::DirectoryIterator::operator!=(const DirectoryIterator &other) const
    {
        return m_iterator != other.m_iterator;
    }

    File::DirectoryIterator &File::DirectoryIterator::operator++()
    {
        ++m_iterator;
        return *this;
    }

    File File::DirectoryIterator::operator*() const
    {
        return File(m_iterator->path());
    }

    File::DirectoryIterator File::begin() const
    {
        if (IsDirectory())
        {
            return DirectoryIterator(m_path.string());
        }
        return DirectoryIterator();
    }

    File::DirectoryIterator File::end() const
    {
        return DirectoryIterator();
    }

    // Stream operators
    File &File::operator>>(std::string &str)
    {
        str = ReadString(1024); // Read up to 1024 bytes
        return *this;
    }

    File &File::operator>>(int64_t &value)
    {
        value = ReadInt64();
        return *this;
    }

    File &File::operator>>(double &value)
    {
        value = ReadDouble();
        return *this;
    }

    File &File::operator<<(const std::string &str)
    {
        WriteAllText(str);
        return *this;
    }

    File &File::operator<<(int64_t value)
    {
        WriteInt64(value);
        return *this;
    }

    File &File::operator<<(double value)
    {
        WriteDouble(value);
        return *this;
    }

    // FileReader implementation
    FileReader::FileReader(const File &file) : m_file(file)
    {
        m_file.Open(FileMode::Read);
    }

    FileReader::FileReader(const std::string &path) : m_file(path)
    {
        m_file.Open(FileMode::Read);
    }

    bool FileReader::IsEOF()
    {
        return m_file.GetPosition() >= m_file.GetSize();
    }

    size_t FileReader::GetPosition()
    {
        return m_file.GetPosition();
    }

    size_t FileReader::GetSize() const
    {
        return m_file.GetSize();
    }

    // FileWriter implementation
    FileWriter::FileWriter(const File &file, FileMode mode) : m_file(file)
    {
        m_file.Open(mode);
    }

    FileWriter::FileWriter(const std::string &path, FileMode mode) : m_file(path)
    {
        m_file.Open(mode);
    }

    void FileWriter::Flush()
    {
        // fstream automatically flushes on close
    }

    // MemoryMappedFile implementation (platform-specific stub)
    MemoryMappedFile::MemoryMappedFile() = default;

    MemoryMappedFile::MemoryMappedFile(const std::string &path, size_t offset, size_t length)
    {
        Open(path, offset, length);
    }

    MemoryMappedFile::~MemoryMappedFile()
    {
        Close();
    }

    bool MemoryMappedFile::Open(const std::string &path, size_t offset, size_t length)
    {
        // Platform-specific implementation would go here
        // This is a stub implementation
        return false;
    }

    void MemoryMappedFile::Close()
    {
        if (m_data)
        {
            // Platform-specific cleanup
            m_data = nullptr;
        }
    }

    bool MemoryMappedFile::IsOpen() const
    {
        return m_data != nullptr;
    }

    size_t MemoryMappedFile::GetSize() const
    {
        return m_size;
    }

    const uint8_t *MemoryMappedFile::GetData() const
    {
        return m_data;
    }

    uint8_t *MemoryMappedFile::GetData()
    {
        return m_data;
    }

    void MemoryMappedFile::Flush()
    {
        // Platform-specific flush
    }

    // FileWatcher implementation
    FileWatcher::FileWatcher() = default;

    FileWatcher::~FileWatcher() = default;

    void FileWatcher::Watch(const std::string &path, bool recursive)
    {
        try
        {
            if (fs::exists(path))
            {
                WatchInfo info;
                info.path = path;
                info.exists = true;
                auto ftime = fs::last_write_time(info.path);
                info.lastWriteTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                m_watches.push_back(info);

                if (recursive && fs::is_directory(path))
                {
                    for (const auto &entry : fs::recursive_directory_iterator(path))
                    {
                        WatchInfo subInfo;
                        subInfo.path = entry.path().string();
                        subInfo.exists = true;
                        auto ftime = fs::last_write_time(subInfo.path);
                        subInfo.lastWriteTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

                        m_watches.push_back(subInfo);
                    }
                }
            }
        }
        catch (const std::exception &)
        {
            // Ignore errors
        }
    }

    void FileWatcher::Unwatch(const std::string &path)
    {
        m_watches.erase(
            std::remove_if(m_watches.begin(), m_watches.end(),
                           [&](const WatchInfo &info)
                           { return info.path == path; }),
            m_watches.end());
    }

    void FileWatcher::SetCallback(Callback callback)
    {
        m_callback = std::move(callback);
    }

    void FileWatcher::Update(float deltaTime)
    {
        static float timeAccumulator = 0.0f;
        timeAccumulator += deltaTime;

        // Check every 0.5 seconds
        if (timeAccumulator < 0.5f)
            return;
        timeAccumulator = 0.0f;

        for (auto &watch : m_watches)
        {
            try
            {
                bool exists = fs::exists(watch.path);

                if (exists != watch.exists)
                {
                    if (exists)
                    {
                        watch.exists = true;
                        auto ftime = fs::last_write_time(watch.path);
                        watch.lastWriteTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                        if (m_callback)
                        {
                            m_callback(watch.path, ChangeType::Created);
                        }
                    }
                    else
                    {
                        watch.exists = false;
                        if (m_callback)
                        {
                            m_callback(watch.path, ChangeType::Deleted);
                        }
                    }
                }
                else if (exists)
                {
                    auto ftime = fs::last_write_time(watch.path);
                    auto currentTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    if (currentTime != watch.lastWriteTime)
                    {
                        watch.lastWriteTime = currentTime;
                        if (m_callback)
                        {
                            m_callback(watch.path, ChangeType::Modified);
                        }
                    }
                }
            }
            catch (const std::exception &)
            {
                // File might have been deleted or permissions changed
            }
        }
    }

} // namespace SF::Engine