
#include "GemConsole.h"
#include <regex>

using namespace std;

regex re_cmd("^\\s*(\\w+?)\\s*$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_arg("^\\s*(\\w+?)\\s+(\\w+)\\s*$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg("^\\s*(\\w+?)\\s+\\\"(.+?)\\\"\\s*$", regex_constants::optimize | regex_constants::ECMAScript);

regex re_cmd_arg_arg("^(\\w+?)\\s+(\\w+)\\s+(\\w+)$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_arg_qarg("^(\\w+?)\\s+(\\w+)\\s+\\\"(.+?)\\\"$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg_arg("^(\\w+?)\\s+\\\"(.+?)\\\"\\s+(\\w+)$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg_qarg("^(\\w+?)\\s+\\\"(.+?)\\\"\\s+\\\"(.+?)\\\"$", regex_constants::optimize | regex_constants::ECMAScript);

GemConsole::GemConsole()
    : handlerData(nullptr)
{
    ClearLog();
    memset(InputBuf, 0, sizeof(InputBuf));
    HistoryPos = -1;

    DefineCommands();

    AutoScroll = true;
    ScrollToBottom = false;
}

GemConsole::~GemConsole()
{
    ClearLog();
}

void GemConsole::ClearLog()
{
    Items.clear();
}

void GemConsole::PrintLn(const char* fmt, ...) IM_FMTARGS(2)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    va_end(args);
    Items.push_back(ConsoleEntry(buf, false));
}

void GemConsole::ErrorLn(const char* fmt, ...) IM_FMTARGS(2)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    va_end(args);
    Items.push_back(ConsoleEntry(buf, true));
}

void Strtrim(char* s) 
{ 
    char* str_end = s + strlen(s); 
    while (str_end > s && str_end[-1] == ' ') 
        str_end--; 

    *str_end = 0; 
}

void GemConsole::Draw(ImFont* mono_font)
{
    if (ImGui::SmallButton("Clear")) { ClearLog(); }
    ImGui::SameLine();
    bool copy_to_clipboard = ImGui::SmallButton("Copy");

    ImGui::Separator();

    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    // Options, Filter
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    FilterWidget.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    if (copy_to_clipboard)
        ImGui::LogToClipboard();

    ImGui::PushFont(mono_font);
    for (int i = 0; i < Items.size(); i++)
    {
        const ConsoleEntry item = Items[i];
        if (!FilterWidget.PassFilter(item.Message.c_str()))
            continue;

        // Normally you would store more information in your item than just a string.
        // (e.g. make Items[] an array of structure, store color/type etc.)
        ImVec4 color;
        bool has_color = false;
        if (item.IsError)
        {
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            has_color = true;
        }

        if (has_color)
            ImGui::PushStyleColor(ImGuiCol_Text, color);

        ImGui::TextUnformatted(item.Message.c_str());

        if (has_color)
            ImGui::PopStyleColor();
    }
    ImGui::PopFont();

    if (copy_to_clipboard)
        ImGui::LogFinish();

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);

    ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    // Command-line
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 16);
    ImGui::PushFont(mono_font);
    if (ImGui::InputText("##CommandTextbox", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
    {
        char* s = InputBuf;
        Strtrim(s);
        if (s[0])
            ExecCommand(s);
        strcpy(s, "");
        reclaim_focus = true;
    }
    ImGui::PopFont();

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
}

void GemConsole::ExecCommand(const char* command_line)
{
    PrintLn("# %s\n", command_line);

    // Insert into history. First find match and delete it so it can be pushed to the back.
    // This isn't trying to be smart or optimal.
    HistoryPos = -1;
    for (int i = History.size() - 1; i >= 0; i--)
    {
        if (GemUtil::StringEquals(History[i], command_line, true))
        {
            History.erase(History.begin() + i);
            break;
        }
    }

    History.push_back(std::string(command_line));

    Command command = ParseCommand(command_line);

    if (command.Type != CommandType::None)
        handler(command, *this, handlerData);

    ScrollToBottom = true;
}

/*static*/ int GemConsole::TextEditCallbackStub(ImGuiInputTextCallbackData* data)
{
    GemConsole* console = (GemConsole*)data->UserData;
    return console->TextEditCallback(data);
}

int GemConsole::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    //PrintLn("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
        // Example of TEXT COMPLETION

        // Locate beginning of current word
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_start = word_end;
        while (word_start > data->Buf)
        {
            const char c = word_start[-1];
            if (c == ' ' || c == '\t' || c == ',' || c == ';')
                break;
            word_start--;
        }

        // Build a list of candidates
        std::vector<std::string> candidates;
        for (int i = 0; i < definitions.size(); i++)
            if (strnicmp(definitions[i].Verb.c_str(), word_start, (int)(word_end - word_start)) == 0)
                candidates.push_back(definitions[i].Verb);

        if (candidates.size() == 0)
        {
            // No match
            PrintLn("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
        }
        else if (candidates.size() == 1)
        {
            // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0].c_str());
            data->InsertChars(data->CursorPos, " ");
        }
        else
        {
            // Multiple matches. Complete as much as we can..
            // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
            int match_len = (int)(word_end - word_start);
            for (;;)
            {
                int c = 0;
                bool all_candidates_matches = true;
                for (int i = 0; i < candidates.size() && all_candidates_matches; i++)
                    if (i == 0)
                        c = toupper(candidates[i][match_len]);
                    else if (c == 0 || c != toupper(candidates[i][match_len]))
                        all_candidates_matches = false;
                if (!all_candidates_matches)
                    break;
                match_len++;
            }

            if (match_len > 0)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + match_len);
            }

            // List matches
            PrintLn("Possible matches:\n");
            for (int i = 0; i < candidates.size(); i++)
                PrintLn("- %s\n", candidates[i].c_str());
        }

        break;
    }
    case ImGuiInputTextFlags_CallbackHistory:
    {
        // Example of HISTORY
        const int prev_history_pos = HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (HistoryPos == -1)
                HistoryPos = History.size() - 1;
            else if (HistoryPos > 0)
                HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (HistoryPos != -1)
                if (++HistoryPos >= History.size())
                    HistoryPos = -1;
        }

        // A better implementation would preserve the data on the current input line along with cursor position.
        if (prev_history_pos != HistoryPos)
        {
            const std::string history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str.c_str());
        }
    }
    }
    return 0;
}


Command GemConsole::ParseCommand(const std::string& command_input)
{
#define PARSE_ARG_POS(pos,val) if (!defn.Arg##pos##IsString) { try { \
	int arg_val = GemUtil::ParseNumericString(command.StrArg##pos); \
	command.Arg##pos = arg_val; \
	command.FArg##pos = arg_val; \
} \
catch (invalid_argument&) { command.Type = CommandType::None; } \
catch (out_of_range&) { command.Type = CommandType::None; } }

    using namespace std;

    Command command;
    smatch match;
    string cmd;
    string arg0, arg1;

    if (regex_match(command_input, match, re_cmd_qarg_qarg)
        || regex_match(command_input, match, re_cmd_qarg_arg)
        || regex_match(command_input, match, re_cmd_arg_qarg)
        || regex_match(command_input, match, re_cmd_arg_arg))
    {
        cmd = match[1].str();
        arg0 = match[2].str();
        arg1 = match[3].str();
    }
    else if (regex_match(command_input, match, re_cmd_qarg)
        || regex_match(command_input, match, re_cmd_arg))
    {
        cmd = match[1].str();
        arg0 = match[2].str();
    }
    else if (regex_match(command_input, match, re_cmd))
    {
        cmd = match[1].str();
    }

    for (int i = 0; i < definitions.size(); i++)
    {
        auto& defn = definitions[i];

        if (cmd.compare(defn.Verb) == 0)
        {
            if (defn.NumArgs == 0 && arg0.empty() && arg1.empty())
            {
                command.Type = defn.Type;
                break;
            }

            if (defn.NumArgs == 1 && !arg0.empty() && arg1.empty())
            {
                command.Type = defn.Type;
                command.StrArg0 = arg0;
                PARSE_ARG_POS(0, arg0);
                break;
            }

            if (defn.NumArgs == 2 && !arg0.empty() && !arg1.empty())
            {
                command.Type = defn.Type;
                command.StrArg0 = arg0;
                command.StrArg1 = arg1;
                PARSE_ARG_POS(0, arg0);
                PARSE_ARG_POS(1, arg1);
                break;
            }
        }
    }

    return command;

#undef PARSE_ARG_POS
}

void GemConsole::DefineCommands()
{
#define ADD_COMMAND(str,ty) definitions.push_back(CommandDefn(str, ty))
#define ADD_COMMAND_INT(str,ty) definitions.push_back(CommandDefn(str, ty, false))
#define ADD_COMMAND_STR(str,ty) definitions.push_back(CommandDefn(str, ty, true))

#define ADD_COMMAND_INT_INT(str,ty) definitions.push_back(CommandDefn(str, ty, false, false))
#define ADD_COMMAND_INT_STR(str,ty) definitions.push_back(CommandDefn(str, ty, false, true))
#define ADD_COMMAND_STR_INT(str,ty) definitions.push_back(CommandDefn(str, ty, true, false))
#define ADD_COMMAND_STR_STR(str,ty) definitions.push_back(CommandDefn(str, ty, true, true))

    ADD_COMMAND_STR("open", CommandType::OpenROM);
    ADD_COMMAND("open", CommandType::OpenROM);
    ADD_COMMAND_STR("o", CommandType::OpenROM);
    ADD_COMMAND("o", CommandType::OpenROM);

    ADD_COMMAND("step", CommandType::Step);
    ADD_COMMAND_INT("stepn", CommandType::StepN);
    ADD_COMMAND("vblank", CommandType::StepUntilVBlank);
    ADD_COMMAND_INT_INT("memdump", CommandType::MemDump);

    ADD_COMMAND("bp", CommandType::Breakpoint);
    ADD_COMMAND_INT_INT("bp", CommandType::Breakpoint);

    ADD_COMMAND("rbp", CommandType::ReadBreakpoint);
    ADD_COMMAND_INT("rbp", CommandType::ReadBreakpoint);
    ADD_COMMAND_INT_INT("rbp", CommandType::ReadBreakpoint);
    ADD_COMMAND_INT_STR("rbp", CommandType::ReadBreakpoint);
    ADD_COMMAND("wbp", CommandType::WriteBreakpoint);
    ADD_COMMAND_INT("wbp", CommandType::WriteBreakpoint);
    ADD_COMMAND_INT_INT("wbp", CommandType::WriteBreakpoint);
    ADD_COMMAND_INT_STR("wbp", CommandType::WriteBreakpoint);

    ADD_COMMAND_INT("trace", CommandType::Trace);
    ADD_COMMAND("screenshot", CommandType::Screenshot);
    ADD_COMMAND_STR("screenshot", CommandType::Screenshot);

    ADD_COMMAND("run", CommandType::Run);
    ADD_COMMAND("play", CommandType::Run);
    ADD_COMMAND("pause", CommandType::Pause);

    ADD_COMMAND("rwplay", CommandType::RewindPlay);
    ADD_COMMAND("rwstop", CommandType::RewindStop);

    ADD_COMMAND("save", CommandType::Save);

    ADD_COMMAND("mute", CommandType::APUMute);
    ADD_COMMAND("unmute", CommandType::APUUnmute);
    ADD_COMMAND_INT_INT("chan", CommandType::APUChannelMask);

    ADD_COMMAND_INT("ccm", CommandType::ColourCorrectionMode);
    ADD_COMMAND_INT("brightness", CommandType::Brightness);

    ADD_COMMAND("reset", CommandType::Reset);
    ADD_COMMAND("exit", CommandType::Exit);
}