#include "CommandsWindow.hpp"
#include <Gui/ocornut/imgui.h>
#include <algorithm>
#include <cctype>
#include "ConsoleVariable/ConsoleVariable.hpp"

namespace SF::Engine
{
    CommandWindow::ParsedCmd CommandWindow::Parse(const string &raw)
    {
        ParsedCmd result;
        const char *p   = raw.c_str();
        const char *end = p + raw.size();

        auto skipSpace = [&]
        {
            while (p < end && isspace(*p))
                ++p;
        };
        auto readToken = [&]
        {
            const char *start = p;
            while (p < end && isspace(*p))
                ++p;
            return string(start, static_cast<size_t>(p - start));
        };

        skipSpace();
        if (p < end)
            result.name = readToken();

        skipSpace();
        while (p < end)
        {
            result.args.push_back(readToken());
            skipSpace();
        }
        return result;
    }

    void CommandWindow::RegisterBuiltins()
    {
        auto &reg = CommandletRegistry::Get();

        // "print <msg...>" echos to log
        struct PrintCmd : Commandlet
        {
            vector<LogEntry> *log{};
            void Execute() override
            {
                string msg;
                for (const auto &a: args)
                {
                    msg += a;
                    msg += ' ';
                }
                log->push_back({LogEntry::Level::Info, "[PRINT] " + msg});
            }
        };
        struct HelpCmd : Commandlet
        {
            vector<LogEntry> *log{};
            void Execute() override
            {
                string msg;
                for (const auto &cmd: CommandletRegistry::Get().commandlets_ | views::values)
                {
                    msg += cmd->name + "\n";
                    msg += ' ';
                }
                log->push_back({LogEntry::Level::Info, "[HELP] " + msg});
            }
        };
        const auto printCmd = reg.Register<PrintCmd>();
        // Inject log pointer (you could also use a callback or event bus)
        dynamic_cast<PrintCmd *>(printCmd.get())->log = &m_log;
    }

    shared_ptr<Commandlet> CommandWindow::Execute(const string &input)
    {
        auto [name, args] = Parse(input);

        // Built-in: clear doesn't go through registry
        if (name == "clear")
        {
            m_log.clear();
            return nullptr;
        }

        if (name.starts_with(CommandWindowConsoleVariablePrefix))
        {
            const string fullName = name.substr(CommandWindowConsoleVariablePrefix.size());

            IConsoleVariable *cvar = ConsoleVariableRegistry::Find(string_view(fullName));
            if (!cvar)
            {
                m_log.push_back({LogEntry::Level::Error, "CVar '" + fullName + "' not found."});
                return nullptr;
            }

            if (args.empty())
            {
                // No value given -> treat as a "get"
                m_log.push_back({LogEntry::Level::Info, fullName + " = " + cvar->ValueToString()});
                return nullptr;
            }

            if (!cvar->SetValueFromString(string_view(args[0])))
            {
                m_log.push_back(
                        {LogEntry::Level::Error, "Failed to parse '" + args[0] + "' for CVar '" + fullName + "'."});
                return nullptr;
            }

            m_log.push_back({LogEntry::Level::Ok, fullName + " set to " + args[0]});
            m_history.push_front(input);
            if (m_history.size() > k_maxHistory)
                m_history.pop_back();
            return nullptr;
        }

        auto &reg = CommandletRegistry::Get();
        auto cmd  = reg.FindByName(name);
        if (!cmd)
        {
            m_log.push_back({LogEntry::Level::Error, "'" + name + "' not found in CommandletRegistry."});
            return nullptr;
        }

        cmd->args = std::move(args);
        cmd->Execute();
        m_history.push_front(input);
        if (m_history.size() > k_maxHistory)
            m_history.pop_back();
        return cmd;
    }

    bool CommandWindow::IsInputCmdInRegistry(const string &in) const
    {
        return CommandletRegistry::Get().FindByName(Parse(in).name) != nullptr;
    }

    void CommandWindow::DrawCommandConsole()
    {
        ImGui::SetNextWindowSize({640, 480}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Command Console"))
        {
            ImGui::End();
            return;
        }

        // Log region
        const float footerH = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##log", {0, -footerH}, false, ImGuiWindowFlags_HorizontalScrollbar);

        for (auto &entry: m_log)
        {
            ImVec4 col = [&]
            {
                switch (entry.level)
                {
                    case LogEntry::Level::Ok:
                        return ImVec4(0.24f, 0.78f, 0.49f, 1.f);
                    case LogEntry::Level::Warning:
                        return ImVec4(0.95f, 0.63f, 0.15f, 1.f);
                    case LogEntry::Level::Error:
                        return ImVec4(0.88f, 0.29f, 0.29f, 1.f);
                    default:
                        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
                }
            }();
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }

        if (m_scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;

        ImGui::EndChild();
        ImGui::Separator();

        // Input row
        char buf[512]      = {};
        const auto copyLen = min<size_t>(m_inputBuf.size(), sizeof(buf) - 1);
        copy_n(m_inputBuf.begin(), copyLen, buf);

        ImGui::PushItemWidth(-60.f);
        bool reclaim = false;
        if (ImGui::InputText(
                    "##input", buf, sizeof(buf),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
                    [](ImGuiInputTextCallbackData *data) -> int
                    {
                        // Arrow-key history callback
                        auto *self      = static_cast<CommandWindow *>(data->UserData);
                        static int hIdx = -1;
                        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
                        {
                            if (data->EventKey == ImGuiKey_UpArrow && hIdx + 1 < (int) self->m_history.size())
                                data->InsertChars(0, self->m_history[++hIdx].c_str());
                            else if (data->EventKey == ImGuiKey_DownArrow && hIdx > 0)
                                data->InsertChars(0, self->m_history[--hIdx].c_str());
                        }
                        return 0;
                    },
                    this))
        {
            m_inputBuf = buf;
            if (!m_inputBuf.empty())
            {
                m_log.push_back({LogEntry::Level::Info, "> " + m_inputBuf});
                Execute(m_inputBuf);
                m_inputBuf.clear();
                m_scrollToBottom = true;
            }
            reclaim = true;
        }
        ImGui::PopItemWidth();
        m_inputBuf = buf;

        ImGui::SameLine();
        if (ImGui::Button("Exec") || reclaim)
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::End();
    }
} // namespace SF::Engine
