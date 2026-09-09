#pragma once
#include <Gui/UIRegistry.hpp>
#include <deque>
#include "Commands.hpp"

namespace SF::Engine
{
    using namespace std;
    class CommandWindow
    {
    public:
        CommandWindow()
        {
            m_uiHandle = UIRegistry::Get().Register([this] { DrawCommandConsole(); });
            RegisterBuiltins();
        }

        ~CommandWindow() { UIRegistry::Get().Unregister(m_uiHandle); }

        void DrawCommandConsole();

        // Returns nullptr if input is invalid / not found
        std::shared_ptr<Commandlet> Execute(const std::string &input);

        void StopAllExecution();

        bool IsInputCmdInRegistry(const string &in) const;

    private:
        void RegisterBuiltins();
        void ParseAndExecute(const string &raw);

        // Parsed input: "scene.open levels/test.scene" → {"scene.open", {"levels/test.scene"}}
        struct ParsedCmd
        {
            string name;
            vector<string> args;
        };
        static ParsedCmd Parse(const string &raw);

        size_t m_uiHandle;       // whatever UIRegistry::Register returns
        deque<string> m_history; // arrow-key recall
        string m_inputBuf;       // ImGui InputText buffer
        string m_pendingExec;    // set when user hits Enter, consumed next frame
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
            string text;
        };
        vector<LogEntry> m_log;

        static constexpr size_t k_maxHistory = 64;
    };
} // namespace SF::Engine
