#pragma once
#include "Commands.hpp"
#include <Gui/UIRegistry.hpp>
#include <string>
#include <vector>
#include <deque>

namespace SF::Engine
{
    class CommandWindow
    {
    public:
        CommandWindow()
        {
            m_uiHandle = UIRegistry::Get().Register([this]
                                                    { DrawCommandConsole(); });
            RegisterBuiltins();
        }

        ~CommandWindow()
        {
            UIRegistry::Get().Unregister(m_uiHandle);
        }

        void DrawCommandConsole();

        // Returns nullptr if input is invalid / not found
        std::shared_ptr<Commandlet> Execute(const std::string &input);

        void StopAllExecution();

        bool IsInputCmdInRegistry(const std::string &in) const;

    private:
        void RegisterBuiltins();
        void ParseAndExecute(const std::string &raw);

        // Parsed input: "scene.open levels/test.scene" → {"scene.open", {"levels/test.scene"}}
        struct ParsedCmd
        {
            std::string name;
            std::vector<std::string> args;
        };
        static ParsedCmd Parse(const std::string &raw);

        std::size_t m_uiHandle;            // whatever UIRegistry::Register returns
        std::deque<std::string> m_history; // arrow-key recall
        std::string m_inputBuf;            // ImGui InputText buffer
        std::string m_pendingExec;         // set when user hits Enter, consumed next frame
        bool m_scrollToBottom = false;

        struct LogEntry
        {
            enum class Level
            {
                Info,
                Ok,
                Warning,
                Error
            } level;
            std::string text;
        };
        std::vector<LogEntry> m_log;

        static constexpr size_t k_maxHistory = 64;
    };
}