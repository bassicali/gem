#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <mutex>

#include "DArray.h"
#include "Core/Gem.h"
#include "GemWindow.h"
#include "GemSoundStream.h"
#include "GemConsole.h"

union StepCommandParams
{
	int NSteps;
	bool UntilVBlank;
};

struct Breakpoint
{
	uint16_t Addr;
	bool Enabled;

	Breakpoint() : Addr(0), Enabled(false)
	{
	}

	Breakpoint(uint16_t addr) : Addr(addr), Enabled(true)
	{
	}
};

// Used to pass information between console and ui thread. Console thread
// set Changed=true when it was window loop to action on it.
struct ThreadSyncState
{
	bool Reset;
	bool Shutdown;
	bool ShowSpritesWindow;
	bool ShowPalettesWindow;
	bool ShowTilesWindow;
	int FrameRateLimit;
	bool ShowFPS;
	std::string ROMPath;
	bool WindowLoopFinished;
	bool Changed;

	CommandType StepType; // 'Unknown' is the 'dont care' value
	StepCommandParams StepParams;

	ThreadSyncState()
		: Reset(false),
			Shutdown(false),
			ShowSpritesWindow(false),
			ShowPalettesWindow(false),
			ShowTilesWindow(false),
			FrameRateLimit(0),
			ShowFPS(false),
			WindowLoopFinished(false),
			Changed(false),
			StepType(CommandType::None)
	{
		StepParams = {1};
	}
};

class GemApp
{
	public:
		GemApp();
		~GemApp();
		bool Init(std::vector<std::string> args);
		bool InitCore();
		void Shutdown();
		bool ShutdownVideo();
		void WindowLoop();
		void ConsoleLoop();
		const Gem& GetCore() const { return core; }

	private:
		bool ShouldEmulateCGBMode();
		
		GemSoundStream sound;
		string rom_file;

		Gem core;
		GemConsole console;
		ThreadSyncState thstate;
		std::timed_mutex console_ui_mtx;
		std::atomic<bool> paused;
		std::vector<Breakpoint> breakpoints;

		void SetWindowTitles();
		void OpenCloseWindows();
		void ProcessEvents();
		void WindowEventHandler(SDL_WindowEvent& ev);
		void DropEventHandler(SDL_DropEvent& ev);
		void KeyPressHandler(SDL_KeyboardEvent& ev);
		void KeyReleaseHandler(SDL_KeyboardEvent& ev);
		
		std::string drop_event_file;
		void HandleDropEventFileChanged();

		bool LoopWork();
		bool DebuggerTick(bool emu_paused);
		void PresentWindows();

		GemWindow* main_window;
		GemWindow* tiles_window;
		GemWindow* sprites_window;
		GemWindow* palettes_window;
		std::vector<GemWindow*> windows;
};