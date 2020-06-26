#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "Util.h"
#include "GemConsole.h"

#define HEX_TWO_DIGIT hex << right << uppercase << setw(2) << setfill('0')
#define HEX_FOUR_DIGIT hex << right << uppercase << setw(4) << setfill('0')
#define SINGLE_BIT(b) ((b) == 1 ? "*" : "_")
#define SINGLE_BOOL(b) ((b) == true ? "*" : "_")
#define UNDO dec << nouppercase << setfill(' ') << left

#define BORDER_COLOR	InWhite
#define LABEL_COLOR		InBrightPurple
#define VALUE_COLOR		InWhite
#define HEADING_COLOR	InBrightPurple
#define SUBTEXT_COLOR	InBlack
#define PROMPT_COLOR	InBlack

using namespace std;
using namespace GemCmdUtil;

// IO Manipulators
ostream& InWhite(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_15); return out; }
ostream& InLightGrey(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_7); return out; }
ostream& InDarkGrey(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_8); return out; }
ostream& InRed(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_4); return out; }
ostream& InGreen(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_2); return out; }
ostream& InOrange(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_12); return out; }
ostream& InBlue(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_1); return out; }
ostream& InBrightBlue(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_9); return out; }
ostream& InYellow(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_14); return out; }
ostream& InPurple(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_5); return out; }
ostream& InBrightPurple(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_13); return out; }
ostream& InBlack(ostream& out) { GemConsole::SetTextColor(PALETTE_KEY_0); return out; }

HANDLE GemConsole::hStdout = nullptr;

regex re_cmd("^\\s*(\\w+?)\\s*$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_arg("^\\s*(\\w+?)\\s+(\\w+)\\s*$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg("^\\s*(\\w+?)\\s+\\\"(.+?)\\\"\\s*$", regex_constants::optimize | regex_constants::ECMAScript);

regex re_cmd_arg_arg("^(\\w+?)\\s+(\\w+)\\s+(\\w+)$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_arg_qarg("^(\\w+?)\\s+(\\w+)\\s+\\\"(.+?)\\\"$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg_arg("^(\\w+?)\\s+\\\"(.+?)\\\"\\s+(\\w+)$", regex_constants::optimize | regex_constants::ECMAScript);
regex re_cmd_qarg_qarg("^(\\w+?)\\s+\\\"(.+?)\\\"\\s+\\\"(.+?)\\\"$", regex_constants::optimize | regex_constants::ECMAScript);

GemConsole::GemConsole()
{
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);	
	if (hStdout == INVALID_HANDLE_VALUE)
		throw exception("Failed to get STDOUT handle");

	CONSOLE_SCREEN_BUFFER_INFOEX ciex;
	ciex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	if (GetConsoleScreenBufferInfoEx(hStdout, &ciex) == FALSE)
		throw exception("Failure while calling GetConsoleScreenBufferInfoEx()");
	
	// Color palette
	ciex.ColorTable[PALETTE_KEY_15] = RGB(254, 240, 255);
	ciex.ColorTable[PALETTE_KEY_1] = RGB(0, 96, 135);
	ciex.ColorTable[PALETTE_KEY_9] = RGB(22, 148, 188);
	ciex.ColorTable[PALETTE_KEY_0] = RGB(68, 68, 68);
	ciex.ColorTable[PALETTE_KEY_12] = RGB(215, 0, 95);
	ciex.ColorTable[PALETTE_KEY_2] = RGB(113, 140, 0);
	ciex.ColorTable[PALETTE_KEY_4] = RGB(223, 0, 0);
	ciex.ColorTable[PALETTE_KEY_5] = RGB(150, 126, 199);
	ciex.ColorTable[PALETTE_KEY_13] = RGB(214, 200, 255);

	// This causes the palette to take effect right away. 
	// Otherwise, the new colors won't appear until the next time foreground attributes are set (or so it appears)
	ciex.wAttributes = PALETTE_KEY_5 << 4;

	// Buffer size
	ciex.dwSize.X = BUFFER_WIDTH; // With +1 the console double spaces the lines
	ciex.dwSize.Y = BUFFER_HEIGHT;

	if (SetConsoleScreenBufferInfoEx(hStdout, &ciex) == FALSE)
		throw exception("Failure while calling SetConsoleScreenBufferInfoEx()");

	// Window size
	SMALL_RECT win_rect;
	win_rect.Left = 0;
	win_rect.Right = WINDOW_WIDTH;
	win_rect.Top = 0;
	win_rect.Bottom = WINDOW_HEIGHT;

	SetConsoleWindowInfo(hStdout, TRUE, &win_rect);
	if (SetConsoleWindowInfo(hStdout, TRUE, &win_rect) == FALSE)
		throw exception("Failure while calling SetConsoleWindowInfo()");

	// Font
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = FONT_SIZE;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_SEMIBOLD;
	wcscpy_s(cfi.FaceName, L"Consolas");
	if (SetCurrentConsoleFontEx(hStdout, FALSE, &cfi) == FALSE)
		throw exception("Failure when calling SetCurrentConsoleFontEx()");

	// After setting the colour attribs the background still doesn't change colour unless you print
	// something to it (or if you resize the window)
	COORD corner = {0};
	SetConsoleCursorPosition(hStdout, corner);
	string empty_line(BUFFER_WIDTH, ' ');
	for (int i = 0; i < BUFFER_HEIGHT + 1; i++)
		cout << empty_line;
	SetConsoleCursorPosition(hStdout, corner);

	SetConsoleTitleA("Gem Console");

	DefineCommands();
}

void GemConsole::PrintLn(const char* fmt...)
{
	va_list args;
	va_start(args, fmt);
	VPrintLn(fmt, args);
	va_end(args);
}

void GemConsole::VPrintLn(const char* fmt, va_list args)
{
	static thread_local char msg_buffer[256];
	vsnprintf(msg_buffer, 256, fmt, args);
	cout << InWhite << msg_buffer << endl;
}

/*static*/
void GemConsole::PrintPrompt()
{
	cout << PROMPT_COLOR << "Enter Command: " << InWhite;
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

	ADD_COMMAND_STR("show", CommandType::ShowWindow);
	ADD_COMMAND_STR("hide", CommandType::HideWindow);

	ADD_COMMAND_STR("print", CommandType::PrintInfo);
	ADD_COMMAND_STR("p", CommandType::PrintInfo);

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

	ADD_COMMAND("save", CommandType::Save);

	ADD_COMMAND("mute", CommandType::APUMute);
	ADD_COMMAND("unmute", CommandType::APUUnmute);
	ADD_COMMAND_INT_INT("chan", CommandType::APUChannelMask);

	ADD_COMMAND_INT("fr", CommandType::SetFrameRateLimit);
	ADD_COMMAND_INT("fps", CommandType::ShowFPS);
	ADD_COMMAND_INT("ccm", CommandType::ColourCorrectionMode);
	ADD_COMMAND_INT("brightness", CommandType::Brightness);

	ADD_COMMAND("reset", CommandType::Reset);
	ADD_COMMAND("exit", CommandType::Exit);
}

ConsoleCommand GemConsole::GetCommand()
{
#define PARSE_ARG_POS(pos,val) if (!defn.Arg##pos##IsString) { try { \
	int arg_val = ParseNumericString(command.StrArg##pos); \
	command.Arg##pos = arg_val; \
	command.FArg##pos = arg_val; \
} \
catch (invalid_argument&) { command.Type = CommandType::None; } \
catch (out_of_range&) { command.Type = CommandType::None; } }
	
	string command_input;
	getline(cin, command_input);

	ConsoleCommand command;
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

void GemConsole::PrintGPUInfo(Gem& gem)
{
#define LCDC 			SUBTEXT_COLOR << "(" << HEX_TWO_DIGIT << unsigned int(gem.GetGPU()->GetLCDControl().ReadByte()) << UNDO << ")"
#define LCDSTAT 		SUBTEXT_COLOR << "(" << HEX_TWO_DIGIT << unsigned int(gem.GetGPU()->GetLCDStatus().ReadByte()) << UNDO << ")"
#define LCDC_EN			left << setw(7) << gem.GetGPU()->GetLCDControl().Enabled
#define LCDC_WINMAP		left << setw(7) << gem.GetGPU()->GetLCDControl().WindowTileMapSelect
#define LCDC_WINEN		left << setw(7) << gem.GetGPU()->GetLCDControl().WindowEnabled
#define LCDC_BGWTILES	left << setw(7) << gem.GetGPU()->GetLCDControl().BgWindowTileDataSelect
#define LCDC_BGMAP		left << setw(7) << gem.GetGPU()->GetLCDControl().BgTileMapSelect
#define LCDC_OBJSIZE	left << setw(7) << (gem.GetGPU()->GetLCDControl().SpriteSize == 1 ? "8x16" : "8x8")
#define LCDC_OBJEN		left << setw(7) << gem.GetGPU()->GetLCDControl().SpriteEnabled
#define LCDC_BGEN		left << setw(24) << gem.GetGPU()->GetLCDControl().BgDisplay
#define LCDS_LYCINT		left << setw(5) << gem.GetGPU()->GetLCDStatus().LycLyCoincidenceIntEnabled
#define LCDS_HBINT		left << setw(5) << gem.GetGPU()->GetLCDStatus().HBlankIntEnabled
#define LCDS_VBINT		left << setw(5) << gem.GetGPU()->GetLCDStatus().VBlankIntEnabled
#define LCDS_OAMINT		left << setw(5) << gem.GetGPU()->GetLCDStatus().OamIntEnabled
#define LCDS_LYCLY		left << setw(5) << gem.GetGPU()->GetLCDStatus().LycLyCoincidence
#define LCDS_MODE		left << setw(5) << unsigned int(static_cast<uint8_t>(gem.GetGPU()->GetLCDStatus().Mode))
#define LCD_LINEY		HEX_TWO_DIGIT << unsigned int(gem.GetGPU()->GetLCDPositions().LineY) << UNDO << "   "
	
	cout << LABEL_COLOR << "    LCDC " << LCDC << LABEL_COLOR << "         Status " << LCDSTAT << endl;
	cout << LABEL_COLOR << " LCD En       " << VALUE_COLOR << LCDC_EN << LABEL_COLOR << "LYC Int     " << VALUE_COLOR << LCDS_LYCINT << endl;
	cout << LABEL_COLOR << " Win Map      " << VALUE_COLOR << LCDC_WINMAP << LABEL_COLOR << "OAM Int     " << VALUE_COLOR << LCDS_OAMINT << endl;
	cout << LABEL_COLOR << " Win En       " << VALUE_COLOR << LCDC_WINEN << LABEL_COLOR << "VB Int      " << VALUE_COLOR << LCDS_VBINT << endl;
	cout << LABEL_COLOR << " BG+Win Tiles " << VALUE_COLOR << LCDC_BGWTILES << LABEL_COLOR << "HB Int      " << VALUE_COLOR << LCDS_HBINT << endl;
	cout << LABEL_COLOR << " BG Map       " << VALUE_COLOR << LCDC_BGMAP << LABEL_COLOR << "LYC=LY      " << VALUE_COLOR << LCDS_LYCLY << endl;
	cout << LABEL_COLOR << " Obj Size     " << VALUE_COLOR << LCDC_OBJSIZE << LABEL_COLOR << "(LY)        " << VALUE_COLOR << LCD_LINEY << endl;
	cout << LABEL_COLOR << " Obj En       " << VALUE_COLOR << LCDC_OBJEN << LABEL_COLOR << "Mode        " << VALUE_COLOR << LCDS_MODE << endl;
	cout << LABEL_COLOR << " BG En        " << VALUE_COLOR << LCDC_BGEN << endl;
}

void GemConsole::PrintCPUInfo(Gem& gem)
{
#define CPU_PC 			HEX_FOUR_DIGIT << gem.GetCPU().GetPC() << UNDO
#define CPU_SP 			HEX_FOUR_DIGIT << gem.GetCPU().GetSP() << UNDO
#define CPU_AF			HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterA()) << "," << HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterF()) << UNDO
#define CPU_BC			HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterB()) << "," << HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterC()) << UNDO
#define CPU_DE			HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterD()) << "," << HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterE()) << UNDO
#define CPU_HL			HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterH()) << "," << HEX_TWO_DIGIT << unsigned int(gem.GetCPU().GetRegisterL()) << UNDO
#define CPU_Z_FLAG		SINGLE_BIT(gem.GetCPU().GetZeroFlag())
#define CPU_C_FLAG		SINGLE_BIT(gem.GetCPU().GetCarryFlag())
#define CPU_H_FLAG		SINGLE_BIT(gem.GetCPU().GetHalfCarryFlag())
#define CPU_N_FLAG		SINGLE_BIT(gem.GetCPU().GetOperationFlag())
#define CPU_FLAGS		CPU_Z_FLAG << " " << CPU_N_FLAG  << " " << CPU_H_FLAG << " " << CPU_C_FLAG
#define CPU_IME			left << setw(7) << (gem.GetCPU().IsInterruptsEnabled() ? "true" : "false")
#define CPU_CALLS		" (" << left << setw(2) << gem.GetCPU().GetCallsOnStack() << " calls)"
#define VBLANK_INT		SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->VBlankEnabled) << " " << SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->VBlankRequested)
#define JOYPAD_INT		SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->JoypadEnabled) << " " << SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->JoypadRequested)
#define SERIAL_INT		SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->SerialEnabled) << " " << SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->SerialRequested)
#define TIMER_INT		SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->TimerEnabled) << " " << SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->TimerRequested)
#define LCDSTAT_INT		SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->LCDStatusEnabled) << " " << SINGLE_BOOL(gem.GetMMU()->GetInterruptController()->LCDStatusRequested)

	cout << LABEL_COLOR << " PC  " << VALUE_COLOR << CPU_PC << LABEL_COLOR << "             IME:   " << VALUE_COLOR << CPU_IME << endl;
	cout << LABEL_COLOR << " SP  " << VALUE_COLOR << CPU_SP << SUBTEXT_COLOR << CPU_CALLS << LABEL_COLOR << "          E R   " << endl;
	cout << LABEL_COLOR << " AF  " << VALUE_COLOR << CPU_AF << LABEL_COLOR << "            VBlank  " << VALUE_COLOR << VBLANK_INT << endl;
	cout << LABEL_COLOR << " BC  " << VALUE_COLOR << CPU_BC << LABEL_COLOR << "            LCD     " << VALUE_COLOR << LCDSTAT_INT << endl;
	cout << LABEL_COLOR << " DE  " << VALUE_COLOR << CPU_DE << LABEL_COLOR << "            Timer   " << VALUE_COLOR << TIMER_INT << endl;
	cout << LABEL_COLOR << " HL  " << VALUE_COLOR << CPU_HL << LABEL_COLOR << "            Serial  " << VALUE_COLOR << SERIAL_INT << endl;
	cout << LABEL_COLOR << "                      Joypad  " << VALUE_COLOR << JOYPAD_INT << endl;
	cout << LABEL_COLOR << " Z N H C" << endl;
	cout << VALUE_COLOR << " " << CPU_FLAGS << endl;
}

void GemConsole::PrintAPUInfo(Gem& gem)
{
	stringstream wave_pattern;
	static uint8_t decoded_wave[32];
	gem.GetAPU()->DecodeWaveRAM(decoded_wave);

	for (int i = 0; i < 32; i++)
	{
		wave_pattern << int(decoded_wave[i]);
		if (i != 31)
			wave_pattern << ",";
	}

	cout << LABEL_COLOR << " Wave RAM: " << wave_pattern.str() << endl;
}

void GemConsole::PrintROMInfo(Gem& gem)
{
#define ROM_TITLE		left << setw(10) << (gem.IsROMLoaded() ? gem.GetCartridgeReader()->Properties().Title : "") << setw(14) << " "
#define ROM_SIZE		left << setw(2) << (gem.IsROMLoaded() ? gem.GetCartridgeReader()->Properties().RomSize : 0) << setw(22) << " KB"
#define RAM_SIZE		left << setw(2) << (gem.IsROMLoaded() ? gem.GetCartridgeReader()->Properties().RamSize : 0) << setw(22) << " KB"
#define ROM_TYPE		left << setw(24) << (gem.IsROMLoaded() ? CartridgeReader::ROMTypeString(gem.GetCartridgeReader()->Properties().Type) : "") << setw(22)

	cout << LABEL_COLOR << " Title        " << VALUE_COLOR << ROM_TITLE << endl;
	cout << LABEL_COLOR << " ROM Size     " << VALUE_COLOR << ROM_SIZE << endl;
	cout << LABEL_COLOR << " RAM Size     " << VALUE_COLOR << RAM_SIZE << endl;
	cout << LABEL_COLOR << " Type         " << VALUE_COLOR << ROM_TYPE << endl;
}

void GemConsole::PrintTimerInfo(Gem& gem)
{
#define TIMER_DIV		left << setw(9) << unsigned int(gem.GetMMU()->GetTimerController().Divider)
#define TIMER_CTR		left << setw(9) << unsigned int(gem.GetMMU()->GetTimerController().Counter)
#define TIMER_MODULO	left << setw(8) << unsigned int(gem.GetMMU()->GetTimerController().Modulo)
#define TIMER_CTR_FREQ	left << setw(6) << gem.GetMMU()->GetTimerController().GetCounterFrequency()
#define TICKS			left << setw(11) << gem.GetTickCount()

	cout << LABEL_COLOR << "                   Freq              " << endl;
	cout << LABEL_COLOR << " DIV    " << VALUE_COLOR << TIMER_DIV << "16,384 Hz" << endl;
	cout << LABEL_COLOR << " TIMA   " << VALUE_COLOR << TIMER_CTR << TIMER_CTR_FREQ << " Hz" << endl;
	cout << LABEL_COLOR << " TMA    " << VALUE_COLOR << TIMER_MODULO << LABEL_COLOR << " Ticks: " << VALUE_COLOR << TICKS << endl;
}

void GemConsole::MemDump(MMU& mmu, uint16_t start, int count)
{
	if (count % 16 != 0)
		count = count + 16 - count % 16;

	uint16_t addr = (start / 16) * 16;
	cout << hex << uppercase << setfill('0') << right;
	int data;
	bool stop = false;

	string str_buff(16, ' ');

	for (int ln = 0; ln < (count / 16) && !stop; ln++)
	{
		cout << LABEL_COLOR << " " << setw(4) << addr << " | ";

		for (int i = 0; i < 16; i++, addr++)
		{
			// TODO: I'm not sure if reading everything is a nop
			if (addr >= 0xFF00)
			{
				stop = true;
				break;
			}

			data = mmu.ReadByte(addr);
			str_buff[i] = data;

			if (addr == start)
				cout << SUBTEXT_COLOR << setw(2) << data << LABEL_COLOR << " ";
			else
				cout << setw(2) << data << " ";
		}

		if (!stop)
			cout << "| " << SanitizeString(str_buff, '.');

		cout << endl;
	}

	cout << UNDO;
}

void GemConsole::SetTitle(const char* title)
{
	SetConsoleTitleA(title);
}

/*static*/
BOOL GemConsole::SetTextColor(WORD color)
{
	static CONSOLE_SCREEN_BUFFER_INFOEX ciex;

	color = color | (PALETTE_KEY_5 << 4);
	ciex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

	if (GetConsoleScreenBufferInfoEx(hStdout, &ciex) == FALSE)
		throw exception("Unable to set text color");

	if (ciex.wAttributes == color)
		return TRUE;

	return SetConsoleTextAttribute(hStdout, color);
}