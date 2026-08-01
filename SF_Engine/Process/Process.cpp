#include "PID.hpp"

#ifdef _WIN32

#define NOMINMAX
#include <Windows.h>
#include <tlhelp32.h>
#include <Psapi.h>

namespace SF::Engine
{
    std::string Process::GetName() const
    {
        if (!IsValid())
            return {};
        wchar_t buf[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(Handle, 0, buf, &size))
        {
            std::filesystem::path p(buf);
            return p.filename().string();
        }
        return {};
    }

    std::optional<Process> Process::GetProcessById(ProcessID pid)
    {
        Process p(pid);
        if (!p.IsValid())
            return std::nullopt;
        return p;
    }

    ::SFTL::DynamicArray<Process> Process::GetProcessesByName(std::string_view name)
    {
        ::SFTL::DynamicArray<Process> result;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return result;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                std::filesystem::path exeName(entry.szExeFile);
                if (exeName.string() == name)
                {
                    Process p(ProcessID{entry.th32ProcessID});
                    if (p.IsValid())
                        result.push_back(std::move(p));
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return result;
    }

    ::SFTL::DynamicArray<Process> Process::GetProcesses()
    {
        ::SFTL::DynamicArray<Process> result;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return result;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                Process p(ProcessID{entry.th32ProcessID});
                if (p.IsValid())
                    result.push_back(std::move(p));
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return result;
    }
    Process::~Process()
    {
        if (Handle && Handle != GetCurrentProcess())
            CloseHandle(Handle);
    }

    Process::Process(Process &&other) noexcept
        : Handle(other.Handle), m_PID(other.m_PID)
    {
        other.Handle = nullptr;
        other.m_PID = ProcessID{};
    }

    Process &Process::operator=(Process &&other) noexcept
    {
        if (this != &other)
        {
            if (Handle && Handle != GetCurrentProcess())
                CloseHandle(Handle);
            Handle = other.Handle;
            m_PID = other.m_PID;
            other.Handle = nullptr;
            other.m_PID = ProcessID{};
        }
        return *this;
    }

    Process::Process(ProcessID pid)
        : m_PID(pid)
    {
        if (pid)
        {
            Handle = ::OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION |
                    SYNCHRONIZE |
                    PROCESS_TERMINATE,
                FALSE,
                pid.Value);
        }
    }

    ProcessID Process::GetPID() const noexcept
    {
        return m_PID;
    }

    bool Process::IsValid() const noexcept
    {
        return Handle != nullptr;
    }

    bool Process::IsRunning() const
    {
        if (!IsValid())
            return false;

        DWORD code;
        if (!GetExitCodeProcess(Handle, &code))
            return false;

        return code == STILL_ACTIVE;
    }

    ProcessState Process::GetState() const
    {
        return IsRunning() ? ProcessState::Running : ProcessState::Exited;
    }

    bool Process::Wait()
    {
        if (!IsValid())
            return false;
        return WaitForSingleObject(Handle, INFINITE) == WAIT_OBJECT_0;
    }

    bool Process::Wait(uint32_t timeout)
    {
        if (!IsValid())
            return false;
        return WaitForSingleObject(Handle, timeout) == WAIT_OBJECT_0;
    }

    bool Process::Terminate(int exitCode)
    {
        return TerminateProcess(Handle, static_cast<UINT>(exitCode));
    }

    Process Process::Current()
    {
        Process p;
        p.m_PID = ProcessID{GetCurrentProcessId()};
        p.Handle = GetCurrentProcess();
        return p;
    }

    Process Process::Launch(
        const std::filesystem::path &exe,
        const SFTL::DynamicArray<SFTL::AdvancedString<char>> &,
        const std::filesystem::path &)
    {
        Process result;

        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        std::wstring cmd = exe.wstring();

        if (::CreateProcessW(
                nullptr,
                cmd.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                nullptr,
                &si,
                &pi))
        {
            result.m_PID = ProcessID{pi.dwProcessId};
            result.Handle = pi.hProcess;
            CloseHandle(pi.hThread);
        }

        return result;
    }

    int Process::GetExitCode() const
    {
        DWORD code = 0;
        if (!IsValid() || !GetExitCodeProcess(Handle, &code) || code == STILL_ACTIVE)
            return -1;
        return static_cast<int>(code);
    }
}

#else

#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>

extern char **environ;

namespace SF::Engine
{
    std::string Process::GetName() const
    {
        if (!IsValid())
            return {};
        std::ifstream comm("/proc/" + std::to_string(m_PID.Value) + "/comm");
        std::string name;
        std::getline(comm, name);
        return name;
    }

    std::optional<Process> Process::GetProcessById(ProcessID pid)
    {
        Process p(pid);
        if (!p.IsValid())
            return std::nullopt;
        return p;
    }

    ::SFTL::DynamicArray<Process> Process::GetProcessesByName(std::string_view name)
    {
        ::SFTL::DynamicArray<Process> result;

        DIR *proc = opendir("/proc");
        if (!proc)
            return result;

        dirent *entry;
        while ((entry = readdir(proc)) != nullptr)
        {
            if (!std::isdigit(static_cast<unsigned char>(entry->d_name[0])))
                continue;

            pid_t pid = std::atoi(entry->d_name);
            std::ifstream comm("/proc/" + std::string(entry->d_name) + "/comm");
            std::string procName;
            std::getline(comm, procName);

            if (procName == name)
            {
                Process p(ProcessID{pid});
                if (p.IsValid())
                    result.push_back(std::move(p));
            }
        }

        closedir(proc);
        return result;
    }

    ::SFTL::DynamicArray<Process> Process::GetProcesses()
    {
        ::SFTL::DynamicArray<Process> result;

        DIR *proc = opendir("/proc");
        if (!proc)
            return result;

        dirent *entry;
        while ((entry = readdir(proc)) != nullptr)
        {
            if (!std::isdigit(static_cast<unsigned char>(entry->d_name[0])))
                continue;

            pid_t pid = std::atoi(entry->d_name);
            Process p(ProcessID{pid});
            if (p.IsValid())
                result.push_back(std::move(p));
        }

        closedir(proc);
        return result;
    }

    Process::Process(ProcessID pid)
        : m_PID(pid), Handle(pid.Value)
    {
    }

    ProcessID Process::GetPID() const noexcept
    {
        return m_PID;
    }

    bool Process::IsValid() const noexcept
    {
        return m_PID.IsValid();
    }

    bool Process::IsRunning() const
    {
        if (!IsValid())
            return false;

        return kill(m_PID.Value, 0) == 0;
    }

    ProcessState Process::GetState() const
    {
        return IsRunning() ? ProcessState::Running : ProcessState::Exited;
    }

    bool Process::Wait()
    {
        return waitpid(m_PID.Value, &m_ExitStatus, 0) == m_PID.Value;
    }

    bool Process::Wait(uint32_t)
    {
        return Wait(); // timeout implementation omitted
    }

    bool Process::Terminate(int)
    {
        return kill(m_PID.Value, SIGTERM) == 0;
    }

    Process Process::Current()
    {
        Process p;
        p.m_PID = ProcessID{getpid()};
        p.Handle = p.m_PID.Value;
        return p;
    }

    Process Process::Launch(
        const std::filesystem::path &exe,
        const SFTL::DynamicArray<SFTL::AdvancedString<char>> &,
        const std::filesystem::path &)
    {
        Process result;

        pid_t pid;
        char *argv[] =
            {
                const_cast<char *>(exe.c_str()),
                nullptr};

        if (posix_spawn(
                &pid,
                exe.c_str(),
                nullptr,
                nullptr,
                argv,
                environ) == 0)
        {
            result.m_PID = ProcessID{pid};
            result.Handle = pid;
        }

        return result;
    }
    Process::~Process()
    {
        // POSIX: Handle is just the pid_t itself, nothing to release.
    }

    Process::Process(Process &&other) noexcept
        : Handle(other.Handle), m_PID(other.m_PID), m_ExitStatus(other.m_ExitStatus)
    {
        other.m_PID = ProcessID{};
    }

    Process &Process::operator=(Process &&other) noexcept
    {
        if (this != &other)
        {
            Handle = other.Handle;
            m_PID = other.m_PID;
            m_ExitStatus = other.m_ExitStatus;
            other.m_PID = ProcessID{};
        }
        return *this;
    }

    int Process::GetExitCode() const
    {
        if (WIFEXITED(m_ExitStatus))
            return WEXITSTATUS(m_ExitStatus);
        return -1;
    }
}
#endif
