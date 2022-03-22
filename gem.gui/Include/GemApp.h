#pragma once

#include <vector>
#include <string>

#include "DArray.h"
#include "Core/Gem.h"
#include "RenderWindow.h"
#include "GemSoundStream.h"
#include "MsgPad.h"
#include "GemDebugger.h"
#include "GemConsole.h"

class GemApp
{
	public:
		GemApp();
		~GemApp();
		bool Init(std::vector<std::string> args);
		bool InitCore();
		void Shutdown();
		void WindowLoop();
		const Gem& GetCore() const { return core; }

	private:

		bool ShouldEmulateCGBMode();
		
		GemSoundStream sound;

		Gem core;

		void SetWindowTitles();
		void ProcessEvents();
		void WindowEventHandler(SDL_WindowEvent& ev);
		void DropEventHandler(SDL_DropEvent& ev);
		void KeyPressHandler(SDL_KeyboardEvent& ev);
		void KeyReleaseHandler(SDL_KeyboardEvent& ev);
		
		std::string drop_event_file;
		void HandleDropEventFileChanged();

		bool LoopWork();
		bool DebuggerTick(bool emu_paused);

		RenderWindow* mainWindow;
		GemDebugger debugger;
};