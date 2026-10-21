#pragma once
#include <UtilityClasses/Patterns.hpp>
#include "Process.hpp"

namespace SF::Engine
{
    class ProcessBuilder : public BuilderPattern<ProcessBuilder, Process>
    {
    public:
        ProcessBuilder &Executable(filesystem::path exe)
        {
            executable_ = std::move(exe);
            return Self();
        }

        ProcessBuilder &Arguments(vector<string> args)
        {
            arguments_ = std::move(args);
            return Self();
        }

        ProcessBuilder &Argument(string arg)
        {
            arguments_.push_back(std::move(arg));
            return Self();
        }

        ProcessBuilder &WorkingDirectory(filesystem::path dir)
        {
            workingDirectory_ = std::move(dir);
            return Self();
        }

        [[nodiscard]] Process Build() const { return Process::Launch(executable_, arguments_, workingDirectory_); }

    private:
        filesystem::path executable_;
        vector<string> arguments_;
        filesystem::path workingDirectory_;
    };
} // namespace SF::Engine
