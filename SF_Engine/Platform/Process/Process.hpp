#pragma once
#include <filesystem>
#include <string>

#include <Platform/PlatformIncludes.hpp>

namespace SF::Engine
{
    using namespace std;
    struct ProcessID
    {
        Platform::NativePID Value = Platform::InvalidPID;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return Value != Platform::InvalidPID; }

        constexpr explicit operator bool() const noexcept { return IsValid(); }

        friend constexpr bool operator==(ProcessID, ProcessID) noexcept = default;
    };

    enum class ProcessState
    {
        Invalid,
        Running,
        Suspended,
        Exited
    };

    class Process
    {
    private:
        Platform::NativeProcessHandle Handle = Platform::InvalidProcessHandle;

    public:
        Process() = default;
        explicit Process(ProcessID pid);
        explicit Process(filesystem::path program, filesystem::path directory) :
            Process(std::move(program), {}, std::move(directory))
        {
        }
        explicit Process(filesystem::path program, vector<string> args, filesystem::path directory)
        {
            *this = Launch(program, args, directory);
        }

        ~Process();

        Process(const Process &)            = delete;
        Process &operator=(const Process &) = delete;

        Process(Process &&other) noexcept;
        Process &operator=(Process &&other) noexcept;

        [[nodiscard]] ProcessID GetPID() const noexcept;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] bool IsRunning() const;
        [[nodiscard]] ProcessState GetState() const;
        [[nodiscard]] int GetExitCode() const;
        [[nodiscard]] string GetName() const;

        bool Wait();
        bool Wait(uint32_t timeoutMilliseconds);

        [[nodiscard]] bool Terminate(int exitCode = 0) const;

        static Process Current();

        static Process Launch(const filesystem::path &executable, const vector<string> &arguments = {},
                              const filesystem::path &workingDirectory = {});

    private:
        ProcessID m_PID{};
        int m_ExitStatus = 0;

    public:
        static optional<Process> GetProcessById(ProcessID pid);
        static vector<Process> GetProcessesByName(string_view name);
        static vector<Process> GetProcesses();
    };
} // namespace SF::Engine
