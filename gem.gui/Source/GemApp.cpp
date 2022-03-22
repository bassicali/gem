
#include <chrono>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <cstdlib>

#include "Util.h"
#include "MsgPad.h"
#include "GemApp.h"
#include "GemConfig.h"
#include "Logging.h"

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

using namespace GemUtil;

GemApp::GemApp()
	: mainWindow(nullptr)
{
	int x = sizeof(GemDebugger);
	int y = 0;
}

GemApp::~GemApp()
{
	if (mainWindow)
		delete mainWindow;
}

bool GemApp::Init(vector<string> args) 
{
	string joined;
	for (int i = 1; i < args.size(); i++)
	{
		string& arg = args[i];
		joined += string(" ") + arg;
	}

	LOG_INFO("Command Line: %s", joined.c_str());
	
	if (args.size() > 1 && args[1][0] != '-')
	{
		fs::path rompath(args[1]);

		if (!fs::exists(rompath))
		{
			LOG_ERROR("ROM file not found: %s", rompath.string().c_str());

			if (GemConfig::Get().ShowConsole)
				GemConsole::Get().PrintLn("ROM file not found");

			return false;
		}

		GMsgPad.ROMPath = rompath.string();
	}

	// This call will load the ini file and initialize the keyboard bindings and dmg colour palette
	GemConfig& config = GemConfig::Get();

	for (string& arg : args)
	{
		if (StringEquals(arg, "--dmg"))
		{
			config.ForceDMGMode = true;
		}
		else if (StringEquals(arg, "--vsync"))
		{
			config.VSync = true;
		}
		else if (StringEquals(arg, "--no-sound"))
		{
			config.NoSound = true;
		}
		else if (StringEquals(arg, "--debugger") || StringEquals(arg, "--debug"))
		{
			config.ShowDebugger = true;
		}
		else if (StringEquals(arg, "--pause"))
		{
			config.PauseAfterOpen = true;
		}
		else if (StringStartsWith(arg, "--res-scale="))
		{
			size_t pos = arg.find_first_of('=');
			if (pos != string::npos)
			{
				string nstr = arg.substr(pos + 1);
				config.ResolutionScale = stof(nstr);
			}
		}
	}

	config.Save();

	LOG_INFO("[GEM] Operating in %s mode", (GemConfig::Get().ForceDMGMode ? "DMG" : "CGB"));
	
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		LOG_ERROR("Failed to initialize SDL video: %s", SDL_GetError());
		return false;
	}

	if (TTF_Init() != 0)
	{
		LOG_ERROR("Failed to initialize SDL TTF: %s", SDL_GetError());
		return false;
	}

	if (config.ShowDebugger)
	{
		if (debugger.Init())
			debugger.SetCore(&core);
		else
			LOG_WARN("Debugger could not be initialized: %s", SDL_GetError());
	}

	if (!InitCore())
		return false;

	if (core.IsROMLoaded())
	{
		GMsgPad.Disassemble.NumInstructions = 100;
		GMsgPad.Disassemble.Address = core.GetCPU().GetPC();
		GMsgPad.Disassemble.State = DisassemblyRequestState::Requested;
	}

	return true;
}

bool GemApp::InitCore()
{
	if (!GemConfig::Get().NoSound)
	{
		if (!sound.IsInitialized() && !sound.Init(core.GetAPU()))
		{
			LOG_ERROR("Sound system could not be initialized, continuing without it");
			core.ToggleSound(false);
		}
	}
	else
	{
		core.ToggleSound(false);
	}
	
	if (!GMsgPad.ROMPath.empty())
	{
		try
		{
			core.LoadRom(GMsgPad.ROMPath.c_str());
		}
		catch (exception& ex)
		{
			cout << "Error while loading rom. Message: " << ex.what();
			return false;
		}

		core.Reset(ShouldEmulateCGBMode());

		shared_ptr<CartridgeReader> rom_reader = core.GetCartridgeReader();

		GemConsole::Get().PrintLn("ROM file loaded");
		GemConsole::Get().PrintLn("Title: %s", rom_reader->Properties().Title);
		GemConsole::Get().PrintLn("ROM Size: %d KB", rom_reader->Properties().ROMSize);
		GemConsole::Get().PrintLn("RAM Size: %d KB", rom_reader->Properties().RAMSize);
		GemConsole::Get().PrintLn("Cartridge: %s", CartridgeReader::ROMTypeString(rom_reader->Properties().Type).c_str());
		GemConsole::Get().PrintLn("CGB: %s", CartridgeReader::CGBSupportString(rom_reader->Properties().CGBCompatability).c_str());
		GemConsole::Get().PrintLn("");
	}
	else if (core.IsROMLoaded())
	{
		core.Reset(ShouldEmulateCGBMode());
	}

	GMsgPad.EmulationPaused.store(GemConfig::Get().PauseAfterOpen || GMsgPad.ROMPath.empty());

	if (debugger.IsInitialized())
		debugger.Reset();

	return true;
}

void GemApp::Shutdown()
{
	GemConfig::Get().Save();

	if (!GemConfig::Get().NoSound)
		sound.Shutdown();
	
	core.Shutdown();

	if (TTF_WasInit())
		TTF_Quit();
}

void GemApp::SetWindowTitles()
{
	bool emu_paused = GMsgPad.EmulationPaused.load();

	stringstream ss1;
	ss1 << "Gem";

	if (core.IsROMLoaded())
		ss1 << " - " << core.GetCartridgeReader()->Properties().Title;

	if (emu_paused)
		ss1 << " (PAUSED)";

	mainWindow->SetTitle(SanitizeString(ss1.str()).c_str());
}

bool GemApp::ShouldEmulateCGBMode()
{
	CGBSupport compat = core.GetCartridgeReader()->GetCGBCompatability();

	return GemConfig::Get().ForceDMGMode
			? false
			: compat == CGBSupport::BackwardsCompatible
				|| compat == CGBSupport::CGBOnly;
}

void GemApp::WindowLoop()
{
	mainWindow = new RenderWindow("", GPU::LCDWidth, GPU::LCDHeight, &sound, GemConfig::Get().VSync, GemConfig::Get().ResolutionScale);

	SetWindowTitles();

	while (mainWindow->IsOpen() && !GMsgPad.Shutdown)
	{
		ProcessEvents();
		
		if (LoopWork())
		{
			mainWindow->DrawFrame(core.GetGPU()->GetFrameBuffer());
			mainWindow->Present();
		}

		if (debugger.IsInitialized() && !debugger.IsHidden())
		{
			debugger.Draw();
		}
		else if (!core.IsROMLoaded())
		{
			// Normally ImGui in the debugger window will limit the framerate, if its closed we have to do it here with a sleep.
			this_thread::sleep_for(16ms);
		}
	}

	sound.Pause();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	exit(0);
}

bool GemApp::LoopWork()
{
	const vector<Breakpoint>& breakpoints = debugger.Breakpoints();

	if (GMsgPad.Reset)
	{
		if (InitCore())
		{
			SetWindowTitles();
		}
		
		GMsgPad.ROMPath.clear();
		GMsgPad.Reset = false;
	}
	else
	{
		HandleDropEventFileChanged();
	}

	if (!core.IsROMLoaded())
		return false;

	bool emu_paused = GMsgPad.EmulationPaused.load();
	bool swap = false;

	if (!emu_paused)
	{
		if (!GemConfig::Get().NoSound && !sound.IsPlaying())
			sound.Play();

		// Emulate the core for 1 frame, handling breakpoints if needed
		if (breakpoints.size() == 0 && !debugger.AnyBreakpoints())
		{
			core.TickUntilVBlank();
			swap = true;
		}
		else
		{
			if (DebuggerTick(false))
				swap = true;
		}
	}
	else if (GMsgPad.StepType != StepType::None)
	{
		if (DebuggerTick(true))
			swap = true;
	}

	if (debugger.IsInitialized())
	{
		debugger.HandleDisassembly(emu_paused);
	}

	return swap;
}

bool GemApp::DebuggerTick(bool emu_paused)
{
	bool hit = false, vblank = false;
	bool stepping_finished = !emu_paused;
	int bp_index = -1, rbp_index = -1, wbp_index = -1;
	const vector<Breakpoint>& rbps = debugger.ReadBreakpoints();
	const vector<Breakpoint>& wbps = debugger.WriteBreakpoints();
	const vector<Breakpoint>& breakpoints = debugger.Breakpoints();
	core.GetMMU()->EvalBreakpoints(true);

	while ((!emu_paused || !stepping_finished) && !hit && !vblank)
	{
		vblank = core.Tick();

		for (int i = 0; i < breakpoints.size(); i++)
		{
			if (breakpoints[i].Enabled && breakpoints[i].Address == core.GetCPU().GetPC())
			{
				bp_index = i;
				hit = true;
				break;
			}
		}

		if (bp_index >= 0)
			break;

		for (int i = 0; i < wbps.size(); i++)
		{
			if (wbps[i].Hit)
			{
				wbp_index = i;
				hit = true;
				break;
			}
		}

		if (wbp_index >= 0)
			break;

		for (int i = 0; i < rbps.size(); i++)
		{
			if (rbps[i].Hit)
			{
				rbp_index = i;
				hit = true;
				break;
			}
		}

		if (rbp_index >= 0)
			break;

		if (emu_paused && GMsgPad.StepType != StepType::None)
		{
			switch (GMsgPad.StepType)
			{
				case StepType::Step:
				case StepType::StepN:
				{
					int& cnt = GMsgPad.StepParams.NSteps;
					if (--cnt == 0)
					{
						GMsgPad.StepType = StepType::None;
						stepping_finished = true;
					}

					break;
				}
			}
		}
	}

	if (bp_index >= 0)
	{
		GMsgPad.EmulationPaused.store(true);
		GemConsole::Get().PrintLn("Breakpoint hit: %04Xh", breakpoints[bp_index].Address);
	}
	else if (wbp_index >= 0)
	{
		GMsgPad.EmulationPaused.store(true);
		const Breakpoint& bp = wbps[wbp_index];

		if (bp.CheckValue)
			GemConsole::Get().PrintLn("MMU Write Breakpoint hit: %04Xh %02Xh", bp.Address, bp.Value);
		else
			GemConsole::Get().PrintLn("MMU Write Breakpoint hit: %04Xh", bp.Address);
	}
	else if (rbp_index >= 0)
	{
		GMsgPad.EmulationPaused.store(true);
		const Breakpoint& bp = rbps[rbp_index];

		if (bp.CheckValue)
			GemConsole::Get().PrintLn("MMU Read Breakpoint hit: %04Xh %02Xh", bp.Address, bp.Value);
		else
			GemConsole::Get().PrintLn("MMU Read Breakpoint hit: %04Xh", bp.Address);
	}
	else if (emu_paused && stepping_finished)
	{
		GMsgPad.EmulationPaused.store(true);
	}

	// While the emulation is technically paused, we still want to refresh some values in the debugger window after 
	// a breakpoint is hit or the 'step' button is clicked.
	if (hit || stepping_finished)
		debugger.RefreshModel = true;

	core.GetMMU()->EvalBreakpoints(false);

	return vblank;
}

void GemApp::ProcessEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.windowID == debugger.SDLWindowId())
					debugger.HandleEvent(event);
				else
					WindowEventHandler(event.window);
				break;

			case SDL_DROPFILE:
				DropEventHandler(event.drop);
				break;

			case SDL_KEYDOWN:
				if (event.key.windowID == debugger.SDLWindowId())
					debugger.HandleEvent(event);
				else
					KeyPressHandler(event.key);
				break;

			case SDL_KEYUP:
				if (event.key.windowID == debugger.SDLWindowId())
					debugger.HandleEvent(event);
				else
					KeyReleaseHandler(event.key);
				break;

			case SDL_MOUSEMOTION:
				if (event.motion.windowID == debugger.SDLWindowId())
					debugger.HandleEvent(event);
				break;

			default:
				if (debugger.IsInitialized())
				{
					debugger.HandleEvent(event);
				}
				break;
		}
	}
}

void GemApp::WindowEventHandler(SDL_WindowEvent& ev)
{
	if (ev.event == SDL_WINDOWEVENT_CLOSE)
	{
		mainWindow->Close();
	}
}

void GemApp::DropEventHandler(SDL_DropEvent& ev)
{
	if (ev.windowID == mainWindow->Id() && filesystem::exists(ev.file))
	{
		drop_event_file = string(ev.file);
	}
}

void GemApp::KeyPressHandler(SDL_KeyboardEvent& ev)
{
	static GemConfig& config = GemConfig::Get();

	if (ev.keysym.sym == config.UpKey)			core.GetJoypad()->Press(JoypadKey::Up);
	else if (ev.keysym.sym == config.DownKey)	core.GetJoypad()->Press(JoypadKey::Down);
	else if (ev.keysym.sym == config.LeftKey)	core.GetJoypad()->Press(JoypadKey::Left);
	else if (ev.keysym.sym == config.RightKey)	core.GetJoypad()->Press(JoypadKey::Right);
	else if (ev.keysym.sym == config.AKey)		core.GetJoypad()->Press(JoypadKey::A);
	else if (ev.keysym.sym == config.BKey)		core.GetJoypad()->Press(JoypadKey::B);
	else if (ev.keysym.sym == config.StartKey)	core.GetJoypad()->Press(JoypadKey::Start);
	else if (ev.keysym.sym == config.SelectKey) core.GetJoypad()->Press(JoypadKey::Select);
}

void GemApp::KeyReleaseHandler(SDL_KeyboardEvent& ev)
{
	static GemConfig& config = GemConfig::Get();

	if (ev.keysym.sym == config.UpKey)			core.GetJoypad()->Release(JoypadKey::Up);
	else if (ev.keysym.sym == config.DownKey)	core.GetJoypad()->Release(JoypadKey::Down);
	else if (ev.keysym.sym == config.LeftKey)	core.GetJoypad()->Release(JoypadKey::Left);
	else if (ev.keysym.sym == config.RightKey)	core.GetJoypad()->Release(JoypadKey::Right);
	else if (ev.keysym.sym == config.AKey)		core.GetJoypad()->Release(JoypadKey::A);
	else if (ev.keysym.sym == config.BKey)		core.GetJoypad()->Release(JoypadKey::B);
	else if (ev.keysym.sym == config.StartKey)	core.GetJoypad()->Release(JoypadKey::Start);
	else if (ev.keysym.sym == config.SelectKey) core.GetJoypad()->Release(JoypadKey::Select);
}

void GemApp::HandleDropEventFileChanged()
{
	if (!drop_event_file.empty())
	{
		GMsgPad.ROMPath = drop_event_file;
		drop_event_file.clear();

		if (InitCore())
			SetWindowTitles();
	}
}