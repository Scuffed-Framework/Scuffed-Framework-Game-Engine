#pragma once
#include <1stPartyLibs/TemplateLibrary/Containers/Deque.hpp>
#include <Gui/UIRegistry.hpp>
#include "Commands.hpp"

namespace SF::Engine
{
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
        std::shared_ptr<Commandlet> Execute(const SFTL::string &input);

        void StopAllExecution();

        bool IsInputCmdInRegistry(const SFTL::string &in) const;

    private:
        void RegisterBuiltins();
        void ParseAndExecute(const ::SFTL::string &raw);

        // Parsed input: "scene.open levels/test.scene" → {"scene.open", {"levels/test.scene"}}
        struct ParsedCmd
        {
            ::SFTL::string name;
            ::SFTL::DynamicArray<::SFTL::string> args;
        };
        static ParsedCmd Parse(const ::SFTL::string &raw);

        ::SFTL::size_type m_uiHandle;            // whatever UIRegistry::Register returns
        ::SFTL::deque<::SFTL::string> m_history; // arrow-key recall
        ::SFTL::string m_inputBuf;               // ImGui InputText buffer
        ::SFTL::string m_pendingExec;            // set when user hits Enter, consumed next frame
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
            SFTL::string text;
        };
        ::SFTL::DynamicArray<LogEntry> m_log;

        static constexpr size_t k_maxHistory = 64;
    };
} // namespace SF::Engine
