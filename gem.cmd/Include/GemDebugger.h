
#include <vector>
#include <string>

#include <SDL_syswm.h>

#include "Core/Gem.h"
#include "Disassembler.h"
#include "GemConsole.h"
#include "OpenGL/PixelUploader.h"

struct UIEditingModel
{
	UIEditingModel(){}
	UIEditingModel(Gem& core);
	void SetValuesFromCore(Gem& core, bool registers = true, bool interrupt_info = true, bool lcd_registers = true, bool apu_registers = true);

	void UpdateCore(Gem& core, bool registers = true, bool interrupt_info = true, bool lcd_registers = true, bool apu_registers = true);

	bool IME;
	bool VBlankEnabled;
	bool VBlankRequested;
	bool LCDEnabled;
	bool LCDRequested;
	bool TimerEnabled;
	bool TimerRequested;
	bool SerialEnabled;
	bool SerialRequested;
	bool JoypadEnabled;
	bool JoypadRequested;

	char Reg_A[3] = { 0, 0, 0 };
	char Reg_F[3] = { 0, 0, 0 };
	char Reg_B[3] = { 0, 0, 0 };
	char Reg_C[3] = { 0, 0, 0 };
	char Reg_D[3] = { 0, 0, 0 };
	char Reg_E[3] = { 0, 0, 0 };
	char Reg_H[3] = { 0, 0, 0 };
	char Reg_L[3] = { 0, 0, 0 };

	char Reg_PC[5] = { 0, 0, 0, 0, 0 };
	char Reg_SP[5] = { 0, 0, 0, 0, 0 };

	bool LCDC_Enabled;
	bool LCDC_WinMap;
	bool LCDC_WinEnabled;
	bool LCDC_TileDataSelect;
	bool LCDC_BGMap;
	bool LCDC_SpriteSize;
	bool LCDC_SpriteEnabled;
	bool LCDC_BGEnabled;
	
	bool LCDS_LYCInt;
	bool LCDS_OAMInt;
	bool LCDS_VBInt;
	bool LCDS_HBInt;
	bool LCDS_LYEqLYC;

	char LCDS_Mode[2] = { 0, 0 };
	char LCDS_LY[5] = { 0, 0, 0, 0, 0 };
	char LCDS_LYC[5] = { 0, 0, 0, 0, 0 };

	bool APU_Mute;
	bool APU_Chan1;
	bool APU_Chan2;
	bool APU_Chan3;
	bool APU_Chan4;

	EmittersSnapshot APU_Snapshot;
};

class GemDebugger
{

public:
	GemDebugger();

	bool Init();
	void Reset();
	void Draw();
	void HandleConsoleCommand(Command& command, GemConsole& console);
	void HandleDisassembly(bool emu_paused);
	void HandleEvent(SDL_Event& event);
	void MemDump(uint16_t start, int count);

	void SetCore(Gem* ptr);
	bool IsInitialized() const { return initialized; }
	bool IsHidden() const { return hidden; }

	int SDLWindowId() const { return windowId; }

	std::vector<Breakpoint>& Breakpoints() { return breakpoints; }

	bool RefreshModel;

private:

	void LayoutWidgets();
	void LayoutGPUVisuals();
	void LayoutAudioVisuals();
	void UpdateDisassembly(uint16_t addr);
	void GenerateDisassemblyText(const DisassemblyChunk& chunk, std::vector<std::string>& out_text);

	Gem* core;
	UIEditingModel model;

	const int WND_WIDTH = 700;
	const int WND_HEIGHT = 580;

	SDL_Window* window;
	SDL_GLContext glContext;
	int windowId;

	bool initialized;
	bool hidden;
	bool running;

	float chan4Wave[32];
	EmittersSnapshot audioSnapshot;

	std::vector<Breakpoint> breakpoints;

	std::vector<std::string> disassemblyTextList;
	DisassemblyChunk* currentDisassemblyChunk;

	SpriteData spriteInfos[GPU::NumSprites];
	ColourBuffer spriteBuffers[GPU::NumSprites];
	PixelUploader spritePxUploaders[GPU::NumSprites];

	CgbTileAttribute tileAttrs[GPU::NumTilesPerSet];
	ColourBuffer tileBuffers[GPU::NumTilesPerSet];
	PixelUploader tilePxUploaders[GPU::NumTilesPerSet];

	ColourPalette::PaletteEntry paletteEntries[GPU::NumPaletteColours];
	ColourBuffer paletteBuffers[GPU::NumPaletteColours];
	PixelUploader palettePxUploaders[GPU::NumPaletteColours];
};