#pragma once
#include <1stPartyLibs/TemplateLibrary/DynamicArray.hpp>
#include <1stPartyLibs/TemplateLibrary/Types.hpp>
#include <filesystem>
#include <string>

#include <Platform/PlatformIncludes.hpp>

namespace SF::Engine
{
    struct ProcessID
    {
        Platform::NativePID Value = Platform::InvalidPID;

        constexpr bool IsValid() const noexcept { return Value != Platform::InvalidPID; }

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
        explicit Process(std::filesystem::path program, std::filesystem::path directory) :
            Process(std::move(program), {}, std::move(directory))
        {
        }
        explicit Process(std::filesystem::path program, ::SFTL::DynamicArray<std::string> args,
                         std::filesystem::path directory)
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
        [[nodiscard]] std::string GetName() const;

        bool Wait();
        bool Wait(uint32_t timeoutMilliseconds);

        bool Terminate(int exitCode = 0);

        static Process Current();

        static Process Launch(const std::filesystem::path &executable,
                              const ::SFTL::DynamicArray<std::string> &arguments = {},
                              const std::filesystem::path &workingDirectory      = {});

    private:
        ProcessID m_PID{};
        int m_ExitStatus = 0;

    public:
        static std::optional<Process> GetProcessById(ProcessID pid);
        static ::SFTL::DynamicArray<Process> GetProcessesByName(std::string_view name);
        static ::SFTL::DynamicArray<Process> GetProcesses();
    };
}