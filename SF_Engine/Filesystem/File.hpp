#ifndef SF_ENGINE_FILE_HPP
#define SF_ENGINE_FILE_HPP

#undef CreateDirectory
#undef GetCurrentDirectory
#undef SetCurrentDirectory

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace SF::Engine
{

    enum class FileMode
    {
        Read,
        Write,
        Append,
        ReadWrite,
        ReadAppend
    };

    enum class FileSeek
    {
        Begin,
        Current,
        End
    };

    enum class FileType
    {
        Regular,
        Directory,
        SymbolicLink,
        Unknown
    };

    struct FileTime
    {
        std::chrono::system_clock::time_point creationTime;
        std::chrono::system_clock::time_point lastAccessTime;
        std::chrono::system_clock::time_point lastWriteTime;
    };

    class File
    {
    public:
        // Constructors
        File() = default;
        explicit File(const std::string& path);
        File(const fs::path& path);
        File(const File& other);
        File(File&& other) noexcept;
        ~File();

        // Assignment operators
        File& operator=(const File& other);
        File& operator=(File&& other) noexcept;

        // File operations
        bool Open(FileMode mode = FileMode::Read);
        void Close();
        bool IsOpen() const;

        // Reading operations
        std::string ReadAllText();
        std::vector<std::string> ReadAllLines();
        std::vector<uint8_t> ReadAllBytes();
        size_t Read(void* buffer, size_t size);
        std::string ReadString(size_t length);
        int64_t ReadInt64();
        int32_t ReadInt32();
        double ReadDouble();
        float ReadFloat();

        // Writing operations
        size_t Write(const void* data, size_t size);
        void WriteAllText(const std::string& text);
        void WriteAllBytes(const std::vector<uint8_t>& data);
        void WriteInt64(int64_t value);
        void WriteInt32(int32_t value);
        void WriteDouble(double value);
        void WriteFloat(float value);

        // Position and size
        size_t GetSize() const;
        size_t GetPosition();
        bool SetPosition(size_t position, FileSeek seek = FileSeek::Begin);
        bool Seek(int64_t offset, FileSeek seek = FileSeek::Current);

        // File system operations
        static bool Exists(const std::string& path);
        bool Exists() const;

        static bool Delete(const std::string& path);
        bool Delete();

        static bool Copy(const std::string& source, const std::string& destination,
                         bool overwrite = false);
        bool CopyTo(const std::string& destination, bool overwrite = false) const;

        static bool Move(const std::string& source, const std::string& destination,
                         bool overwrite = false);
        bool MoveTo(const std::string& destination, bool overwrite = false);

        static bool Rename(const std::string& oldPath, const std::string& newPath);
        bool Rename(const std::string& newName);

        // File information
        std::string GetName() const;
        std::string GetNameWithoutExtension() const;
        std::string GetExtension() const;
        std::string GetDirectory() const;
        std::string GetFullPath() const;
        FileType GetType() const;
        FileTime GetTimes() const;

        bool IsRegularFile() const;
        bool IsDirectory() const;
        bool IsSymbolicLink() const;

        // Directory operations
        static bool CreateDirectory(const std::string& path);
        static bool DeleteDirectory(const std::string& path, bool recursive = false);

        static std::vector<File> GetFiles(const std::string& directory,
                                          const std::string& pattern = "*", bool recursive = false);
        static std::vector<File> GetDirectories(const std::string& directory,
                                                bool recursive = false);

        // Utility functions
        static std::string GetCurrentDirectory();
        static bool SetCurrentDirectory(const std::string& path);
        static std::string GetTempDirectory();
        static File CreateTempFile();

        // Stream-like operations
        File& operator>>(std::string& str);
        File& operator>>(int64_t& value);
        File& operator>>(double& value);

        File& operator<<(const std::string& str);
        File& operator<<(int64_t value);
        File& operator<<(double value);

        operator bool() const;

    private:
        fs::path m_path;
        std::fstream m_stream;
        mutable size_t m_cachedSize = 0;
        bool m_isOpen = false;

        std::ios::openmode ConvertMode(FileMode mode) const;
        void UpdateCache();

    public:
        // Iterator support for directory traversal
        class DirectoryIterator
        {
        public:
            DirectoryIterator() = default;
            explicit DirectoryIterator(const std::string& path);

            DirectoryIterator begin() const;
            DirectoryIterator end() const;

            bool operator!=(const DirectoryIterator& other) const;
            DirectoryIterator& operator++();
            File operator*() const;

        private:
            fs::recursive_directory_iterator m_iterator;
        };

        DirectoryIterator begin() const;
        DirectoryIterator end() const;
    };

    // File wrapper for reading
    class FileReader
    {
    public:
        explicit FileReader(const File& file);
        explicit FileReader(const std::string& path);

        template <typename T>
        T Read();

        template <typename T>
        FileReader& operator>>(T& value);

        bool IsEOF();
        size_t GetPosition();
        size_t GetSize() const;

    private:
        File m_file;
    };

    // File wrapper for writing
    class FileWriter
    {
    public:
        explicit FileWriter(const File& file, FileMode mode = FileMode::Write);
        explicit FileWriter(const std::string& path, FileMode mode = FileMode::Write);

        template <typename T>
        void Write(const T& value);

        template <typename T>
        FileWriter& operator<<(const T& value);

        void Flush();

    private:
        File m_file;
    };

    // Memory-mapped file for high-performance I/O
    class MemoryMappedFile
    {
    public:
        MemoryMappedFile();
        explicit MemoryMappedFile(const std::string& path, size_t offset = 0, size_t length = 0);
        ~MemoryMappedFile();

        bool Open(const std::string& path, size_t offset = 0, size_t length = 0);
        void Close();

        bool IsOpen() const;
        size_t GetSize() const;
        const uint8_t* GetData() const;
        uint8_t* GetData();

        template <typename T>
        const T* GetAs() const;

        void Flush();

    private:
#ifdef _WIN32
        void* m_fileHandle = nullptr;
        void* m_mappingHandle = nullptr;
#else
        int m_fileDescriptor = -1;
#endif
        uint8_t* m_data = nullptr;
        size_t m_size = 0;
        size_t m_offset = 0;
    };

    // File watcher for monitoring file changes
    class FileWatcher
    {
    public:
        enum class ChangeType
        {
            Created,
            Modified,
            Deleted,
            Renamed
        };

        using Callback = std::function<void(const std::string&, ChangeType)>;

        FileWatcher();
        ~FileWatcher();

        void Watch(const std::string& path, bool recursive = true);
        void Unwatch(const std::string& path);

        void SetCallback(Callback callback);
        void Update(float deltaTime);  // Call this periodically

    private:
        struct WatchInfo
        {
            std::string path;
            std::chrono::system_clock::time_point lastWriteTime;
            bool exists;
        };

        std::vector<WatchInfo> m_watches;
        Callback m_callback;
        std::unordered_map<std::string, fs::file_time_type> m_fileTimes;
    };

    // Implementation of inline methods
    inline File::File(const std::string& path) : m_path(path) {}

    inline File::File(const fs::path& path) : m_path(path) {}

    inline bool File::Exists() const
    {
        return fs::exists(m_path);
    }

    inline bool File::Exists(const std::string& path)
    {
        return fs::exists(path);
    }

    inline std::string File::GetName() const
    {
        return m_path.filename().string();
    }

    inline std::string File::GetExtension() const
    {
        return m_path.extension().string();
    }

    inline std::string File::GetFullPath() const
    {
        return fs::absolute(m_path).string();
    }

    inline size_t File::GetSize() const
    {
        if (m_cachedSize == 0 && fs::exists(m_path))
        {
            m_cachedSize = static_cast<size_t>(fs::file_size(m_path));
        }
        return m_cachedSize;
    }

    inline bool File::IsRegularFile() const
    {
        return fs::is_regular_file(m_path);
    }

    inline bool File::IsDirectory() const
    {
        return fs::is_directory(m_path);
    }

    inline File::operator bool() const
    {
        return m_isOpen && m_stream.good();
    }

    // Template implementations
    template <typename T>
    T FileReader::Read()
    {
        T value;
        m_file.Read(&value, sizeof(T));
        return value;
    }

    template <typename T>
    FileReader& FileReader::operator>>(T& value)
    {
        value = Read<T>();
        return *this;
    }

    template <typename T>
    void FileWriter::Write(const T& value)
    {
        m_file.Write(&value, sizeof(T));
    }

    template <typename T>
    FileWriter& FileWriter::operator<<(const T& value)
    {
        Write(value);
        return *this;
    }

    template <typename T>
    const T* MemoryMappedFile::GetAs() const
    {
        return reinterpret_cast<const T*>(m_data);
    }

}  // namespace SF::Engine

#endif  // SF_ENGINE_FILE_HPP