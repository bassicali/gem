
#pragma once

#include <regex>

#include <windows.h>
#include <iostream>

#include "Core/Gem.h"

#define PALETTE_KEY_0	0
#define PALETTE_KEY_1	(FOREGROUND_BLUE)
#define PALETTE_KEY_2	(FOREGROUND_GREEN)
#define PALETTE_KEY_3	(FOREGROUND_GREEN | FOREGROUND_BLUE)
#define PALETTE_KEY_4	(FOREGROUND_RED)
#define PALETTE_KEY_5	(FOREGROUND_RED | FOREGROUND_BLUE)
#define PALETTE_KEY_6	(FOREGROUND_RED | FOREGROUND_GREEN)
#define PALETTE_KEY_7	(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define PALETTE_KEY_8	(FOREGROUND_INTENSITY)
#define PALETTE_KEY_9	(FOREGROUND_INTENSITY | FOREGROUND_BLUE)
#define	PALETTE_KEY_10	(FOREGROUND_INTENSITY | FOREGROUND_GREEN)
#define PALETTE_KEY_11	(FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define PALETTE_KEY_12	(FOREGROUND_INTENSITY | FOREGROUND_RED)
#define PALETTE_KEY_13	(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE)
#define PALETTE_KEY_14	(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN)
#define PALETTE_KEY_15	(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

#define FONT_SIZE 16

#define BUFFER_WIDTH 100
#define BUFFER_HEIGHT 150

#define WINDOW_WIDTH 99
#define WINDOW_HEIGHT 29

enum class CommandType
{
	None,
	OpenROM,
	PrintInfo,
	ShowWindow,
	HideWindow,
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

struct ConsoleCommand
{
	CommandType Type = CommandType::None;
	int Arg0 = 0;
	int Arg1 = 0;
	float FArg0 = 0;
	float FArg1 = 0;
	std::string StrArg0;
	std::string StrArg1;
};

class GemConsole
{
public:
	GemConsole();

	void PrintGPUInfo(Gem& gem);
	void PrintCPUInfo(Gem& gem);
	void PrintAPUInfo(Gem& gem);
	void PrintROMInfo(Gem& gem);
	void PrintTimerInfo(Gem& gem);
	void MemDump(MMU& mmu, uint16_t start, int count);
	void SetTitle(const char* title);
	ConsoleCommand GetCommand();

	static void PrintPrompt();
	static void PrintLn(const char* fmt...);
	static void VPrintLn(const char* fmt, va_list args);
	static BOOL SetTextColor(WORD color);

private:
	static HANDLE hStdout;

	void DefineCommands();
	std::vector<CommandDefn> definitions;
};
