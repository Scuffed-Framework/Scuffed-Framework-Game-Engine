#pragma once
#include <UtilityClasses/Patterns.hpp>
#include "Process.hpp"

namespace SF::Engine
{
    class ProcessBuilder : public BuilderPattern<ProcessBuilder, Process>
    {
    public:
        ProcessBuilder &Executable(std::filesystem::path exe)
        {
            executable_ = std::move(exe);
            return Self();
        }

        ProcessBuilder &Arguments(::SFTL::DynamicArray<std::string> args)
        {
            arguments_ = std::move(args);
            return Self();
        }

        ProcessBuilder &Argument(std::string arg)
        {
            arguments_.push_back(std::move(arg));
            return Self();
        }

        ProcessBuilder &WorkingDirectory(std::filesystem::path dir)
        {
            workingDirectory_ = std::move(dir);
            return Self();
        }

        Process Build() const
        {
            return Process::Launch(executable_, arguments_, workingDirectory_);
        }

    private:
        std::filesystem::path executable_;
        ::SFTL::DynamicArray<std::string> arguments_;
        std::filesystem::path workingDirectory_;
    };
}