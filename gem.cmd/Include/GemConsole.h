
#pragma once

#include <string>
#include <cstring>
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "Core/Gem.h"
#include "Util.h"

enum class CommandType
{
    None,
    OpenROM,
    Step,
    StepN,
    StepUntilVBlank,
    Breakpoint,
    ReadBreakpoint,
    WriteBreakpoint,
    MemDump,
    Trace,
    Screenshot,
    Run,
    Pause,
    Save,
    SetFrameRateLimit,
    ColourCorrectionMode,
    Brightness,
    ShowFPS,
    APUMute,
    APUUnmute,
    APUChannelOn,
    APUChannelMask,
    Reset,
    Exit
};

struct CommandDefn
{
    int NumArgs;
    bool Arg0IsString;
    bool Arg1IsString;

    std::string Verb;
    CommandType Type;

    CommandDefn(const char* str, CommandType ty) : Type(ty), Verb(str), NumArgs(0), Arg0IsString(false), Arg1IsString(false)
    {
    }

    CommandDefn(const char* str, CommandType ty, bool arg0_is_string) : Type(ty), Verb(str), NumArgs(1), Arg0IsString(arg0_is_string), Arg1IsString(false)
    {
    }

    CommandDefn(const char* str, CommandType ty, bool arg0_is_string, bool arg1_is_string) : Type(ty), Verb(str), NumArgs(2), Arg0IsString(arg0_is_string), Arg1IsString(arg1_is_string)
    {
    }
};

struct Command
{
    CommandType Type = CommandType::None;
    int Arg0 = 0;
    int Arg1 = 0;
    float FArg0 = 0;
    float FArg1 = 0;
    std::string StrArg0;
    std::string StrArg1;
};

struct ConsoleEntry
{
    std::string Message;
    bool IsError;

    ConsoleEntry(const char* message, bool is_error = false)
        : Message(message)
        , IsError(is_error)
    {
    }
};

struct GemConsole;

typedef void (*ExecHandler)(Command& command, GemConsole& console, void* data);

// This class is mostly the same as the console example in the imgui demo. 
// See: https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
struct GemConsole
{
    GemConsole();
    ~GemConsole();

    static GemConsole& Get()
    {
        static GemConsole console;
        return console;
    }

    void SetCore(Gem* gem) { core = gem; }
    void SetHandler(ExecHandler h) { handler = h; }
    void SetHandlerUserData(void* data) { handlerData = data; }

    void PrintLn(const char* fmt, ...) IM_FMTARGS(2);
    void ErrorLn(const char* fmt, ...) IM_FMTARGS(2);

    void Draw(ImFont* mono_font);

private:
    void DefineCommands();
    Command ParseCommand(const std::string& command_input);
    void ExecCommand(const char* command_line);

    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
    int TextEditCallback(ImGuiInputTextCallbackData* data);

    void ClearLog();

    char InputBuf[256];
    std::vector<ConsoleEntry> Items;
    std::vector<std::string> History;
    int HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter FilterWidget;
    bool AutoScroll;
    bool ScrollToBottom;

    Gem* core = nullptr;
    std::vector<CommandDefn> definitions;
    ExecHandler handler;
    void* handlerData;
};
