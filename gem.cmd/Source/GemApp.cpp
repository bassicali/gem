
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdlib>

#include "Util.h"
#include "GemApp.h"
#include "Logging.h"
#include "BoostLogger.h"

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

using namespace GemCmdUtil;

#define SCOPE_LOCK_MTX() lock_guard<timed_mutex> lock(console_ui_mtx)

GemApp::GemApp() :
	paused(true),
	main_window(nullptr),
	tiles_window(nullptr),
	sprites_window(nullptr),
	palettes_window(nullptr)
{
	settings.ForceDMGMode = false;
}

GemApp::~GemApp()
{
	if (main_window != nullptr)
	{
		delete main_window;
	}

	if (tiles_window != nullptr)
	{
		delete tiles_window;
	}

	if (sprites_window != nullptr)
	{
		delete sprites_window;
	}
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
	
	bool load_rom = false;
	if (args.size() > 1 && args[1][0] != '-')
	{
		fs::path rompath(args[1]);

		if (!fs::exists(rompath))
		{
			GemConsole::PrintLn("ROM file not found");
			return false;
		}

		rom_file = rompath.string();
		load_rom = true;
	}

	for (string& arg : args)
	{
		if (StringEquals(arg, "--dmg"))
		{
			settings.ForceDMGMode = true;
		}
		else if (StringEquals(arg, "--vsync"))
		{
			settings.VSync = true;
		}
		else if (StringEquals(arg, "--no-sound"))
		{
			settings.NoSound = true;
		}
		else if (StringEquals(arg, "--pause"))
		{
			settings.PauseAfterOpen = true;
		}
		else if (StringStartsWith(arg, "--res-scale="))
		{
			size_t pos = arg.find_first_of('=');
			if (pos != string::npos)
			{
				string nstr = arg.substr(pos + 1);
				settings.ResolutionScale = stof(nstr);
			}
		}
	}

	LOG_INFO("[GEM] Operating in %s mode", (settings.ForceDMGMode ? "DMG" : "CGB"));
	
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

	if (!InitCore())
		return false;

	return true;
}

bool GemApp::InitCore()
{
	if (!settings.NoSound)
	{
		if (!sound.Init(core.GetAPU()))
		{
			LOG_ERROR("Sound system could not be initialized, continuing without it");
			core.ToggleSound(false);
		}
	}
	else
	{
		core.ToggleSound(false);
	}
	
	if (!rom_file.empty())
	{
		try
		{
			core.LoadRom(rom_file.c_str());
		}
		catch (exception& ex)
		{
			cout << "Error while loading rom. Message: " << ex.what();
			return false;
		}

		core.Reset(ShouldEmulateCGBMode());

		shared_ptr<CartridgeReader> rom_reader = core.GetCartridgeReader();
		GemConsole::PrintLn(" ROM file loaded");
		GemConsole::PrintLn(" Title: %s", rom_reader->Properties().Title);
		GemConsole::PrintLn(" ROM Size: %d KB", rom_reader->Properties().RomSize);
		GemConsole::PrintLn(" RAM Size: %d KB", rom_reader->Properties().RamSize);
		GemConsole::PrintLn(" Cartridge: %s", CartridgeReader::ROMTypeString(rom_reader->Properties().Type).c_str());
		GemConsole::PrintLn(" CGB: %s", CartridgeReader::CGBSupportString(rom_reader->Properties().CGBCompatability).c_str());
		GemConsole::PrintLn("");
	}

	paused.store(settings.PauseAfterOpen || rom_file.empty());

	return true;
}

void GemApp::Shutdown()
{
	if (!settings.NoSound)
		sound.Shutdown();
	
	core.Shutdown();
	
	if (TTF_WasInit())
		TTF_Quit();

	if (!ShutdownVideo())
		LOG_ERROR("UI thread could not be shutdown gracefully");
}

bool GemApp::ShutdownVideo()
{
	{
		lock_guard<timed_mutex> lock(console_ui_mtx);
		thstate.Shutdown = true;
		thstate.Changed = true;
	}

	// Give other thread some time to re-evaluate thstate
	this_thread::sleep_for(20ms);

	if (console_ui_mtx.try_lock_for(100ms))
	{
		bool finished = thstate.WindowLoopFinished;
		console_ui_mtx.unlock();
		return finished;
	}

	return false;
}

void GemApp::SetWindowTitles()
{
	stringstream ss1;
	ss1 << "Gem";
	stringstream ss2;
	ss2 << "Gem Console";

	if (core.IsROMLoaded())
	{
		ss1 << " - " << core.GetCartridgeReader()->Properties().Title;
		ss2 << " - " << core.GetCartridgeReader()->Properties().Title;
	}

	if (paused.load() == true)
	{
		ss1 << " (PAUSED)";
		ss2 << " (PAUSED)";
	}

	main_window->SetTitle(SanitizeString(ss1.str()).c_str());
	console.SetTitle(SanitizeString(ss2.str()).c_str());
}

bool GemApp::ShouldEmulateCGBMode()
{
	CGBSupport compat = core.GetCartridgeReader()->GetCGBCompatability();

	return settings.ForceDMGMode
			? false
			: compat == CGBSupport::BackwardsCompatible
				|| compat == CGBSupport::CGBOnly;
}

void GemApp::ConsoleLoop()
{
	bool stop = false;
	bool defer_prompt = false;

	static char path[_MAX_PATH];
	static OPENFILENAMEA opts;

	while (!stop)
	{
		GemConsole::PrintPrompt();
		ConsoleCommand cmd = console.GetCommand();

		switch (cmd.Type)
		{
			case CommandType::OpenROM:
			{
				if (!cmd.StrArg0.empty())
				{
					fs::path load_path(cmd.StrArg0);

					if (!fs::exists(load_path))
					{
						console.PrintLn("ROM file not found");
					}
					else
					{
						SCOPE_LOCK_MTX();
						thstate.ROMPath = fs::absolute(load_path).string();
						thstate.Changed = true;
						thstate.Reset = true;
					}
				}
				else
				{
					memset(&opts, 0, sizeof(OPENFILENAMEA));
					opts.lStructSize = sizeof(OPENFILENAMEA);
					opts.hwndOwner = NULL;
					opts.lpstrFile = &path[0];
					opts.nMaxFile = sizeof(char) * _MAX_PATH;
					opts.lpstrFilter = "Game Boy ROMs (*.gb;*.gbc)\0*.gb;*.gbc\0\0";
					opts.nFilterIndex = 1;
					opts.lpstrFileTitle = NULL;
					opts.nMaxFileTitle = 0;
					opts.lpstrInitialDir = NULL;

					opts.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOVALIDATE;
					if (GetOpenFileNameA(&opts))
					{
						SCOPE_LOCK_MTX();
						thstate.ROMPath = string(path);
						thstate.Changed = true;
						thstate.Reset = true;
					}
				}

				break;
			}

			case CommandType::ShowWindow:
			{
				if (StringEquals(cmd.StrArg0, "palettes"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowPalettesWindow = true;
					thstate.Changed = true;
				}
				else if (StringEquals(cmd.StrArg0, "sprites"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowSpritesWindow = true;
					thstate.Changed = true;
				}
				else if (StringEquals(cmd.StrArg0, "tiles"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowTilesWindow = true;
					thstate.Changed = true;
				}

				break;
			}

			case CommandType::HideWindow:
			{
				if (StringEquals(cmd.StrArg0, "palettes"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowPalettesWindow = false;
					thstate.Changed = true;
				}
				else if (StringEquals(cmd.StrArg0, "sprites"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowSpritesWindow = false;
					thstate.Changed = true;
				}
				else if (StringEquals(cmd.StrArg0, "tiles"))
				{
					SCOPE_LOCK_MTX();
					thstate.ShowTilesWindow = false;
					thstate.Changed = true;
				}

				break;
			}

			case CommandType::PrintInfo:
			{
				SCOPE_LOCK_MTX();

				if (StringEquals(cmd.StrArg0, "lcd") || StringEquals(cmd.StrArg0, "gpu"))
					console.PrintGPUInfo(core);
				else if (StringEquals(cmd.StrArg0, "cpu") || StringEquals(cmd.StrArg0, "z80"))
					console.PrintCPUInfo(core);
				else if (StringEquals(cmd.StrArg0, "apu"))
					console.PrintAPUInfo(core);
				else if (StringEquals(cmd.StrArg0, "timer"))
					console.PrintTimerInfo(core);
				else if (StringEquals(cmd.StrArg0, "rom"))
					console.PrintROMInfo(core);


				break;
			}
			case CommandType::Trace:
			{
				SCOPE_LOCK_MTX();
				if (cmd.Arg0)
				{
					auto& filename = core.StartTrace();
					console.PrintLn("CPU trace started: %s", filename.c_str());
				}
				else
				{
					core.EndTrace();
					console.PrintLn("CPU trace stopped");
				}

				break;
			}
			case CommandType::Run:
			{
				paused.store(false);
				SetWindowTitles();
				break;
			}
			case CommandType::Pause:
			{
				paused.store(true);
				SetWindowTitles();
				break;
			}			
			case CommandType::Save:
			{
				SCOPE_LOCK_MTX();
				shared_ptr<CartridgeReader> cart = core.GetCartridgeReader();
				if (cart)
				{
					if (cart->Properties().ExtRamHasBattery)
					{
						core.GetMMU()->SaveGame();
						console.PrintLn("Save-game written to %s", core.GetCartridgeReader()->SaveGameFile().c_str());
					}
					else
					{
						console.PrintLn("This cartridge type cannot create a save-game");
					}
				}
				else
				{
					console.PrintLn("No cartridge");
				}

				break;
			}
			case CommandType::APUMute:
			{
				SCOPE_LOCK_MTX();
				core.GetAPU()->SetChannelMask(0, 0);
				core.GetAPU()->SetChannelMask(1, 0);
				core.GetAPU()->SetChannelMask(2, 0);
				core.GetAPU()->SetChannelMask(3, 0);
				console.PrintLn("APU muted");
				break;
			}
			case CommandType::APUUnmute:
			{
				SCOPE_LOCK_MTX();
				core.GetAPU()->SetChannelMask(0, 1);
				core.GetAPU()->SetChannelMask(1, 1);
				core.GetAPU()->SetChannelMask(2, 1);
				core.GetAPU()->SetChannelMask(3, 1);
				console.PrintLn("APU unmuted");
				break;
			}
			case CommandType::APUChannelMask:
			{
				SCOPE_LOCK_MTX();
				int index = cmd.Arg0 - 1;
				if (index >= 0 && index <= 3 && (cmd.Arg1 == 0 || cmd.Arg1 == 1))
				{
					core.GetAPU()->SetChannelMask(index, cmd.Arg1);
					console.PrintLn("Channel %d mask: %d", cmd.Arg0, cmd.Arg1);
				}
				else
				{
					console.PrintLn("Invalid command args");
				}
				break;
			}
			case CommandType::ColourCorrectionMode:
			{
				SCOPE_LOCK_MTX();
				switch (cmd.Arg0)
				{
					case 0:
						core.GetGPU()->SetColourCorrectionMode(CorrectionMode::Washout);
						break;
					case 1:
						core.GetGPU()->SetColourCorrectionMode(CorrectionMode::Scale);
						break;
				}
				break;
			}
			case CommandType::Brightness:
			{
				SCOPE_LOCK_MTX();
				core.GetGPU()->SetBrightness(cmd.FArg0);
				break;
			}
			case CommandType::SetFrameRateLimit:
			{
				SCOPE_LOCK_MTX();
				thstate.FrameRateLimit = cmd.Arg0;
				thstate.Changed = true;
				break;
			}
			case CommandType::ShowFPS:
			{
				SCOPE_LOCK_MTX();
				thstate.ShowFPS = (bool)cmd.Arg0;
				thstate.Changed = true;
				break;
			}
			case CommandType::Reset:
			{
				SCOPE_LOCK_MTX();
				thstate.Reset = true;
				thstate.Changed = true;
				break;
			}
			case CommandType::Exit:
			{
				stop = true;

				SCOPE_LOCK_MTX();
				// End the console loop and signal the window loop to also stop
				thstate.Shutdown = true;
				thstate.Changed = true;
				break;
			}

			case CommandType::Breakpoint:
			{
				SCOPE_LOCK_MTX();

				if (cmd.Arg0 == 0 && cmd.Arg1 == 0)
				{
					if (breakpoints.size() == 0)
					{
						console.PrintLn("No breakpoints set");
					}
					else
					{
						console.PrintLn(" Breakpoints");
						for (int i = 0; i < breakpoints.size(); i++)
						{
							if (breakpoints[i].Enabled)
								console.PrintLn(" [%d] %04Xh", i, breakpoints[i].Addr);
							else
								console.PrintLn(" [%d] %04Xh (off)", i, breakpoints[i].Addr);
						}
					}
				}
				else
				{
					bool found = false;
					for (int i = 0; i < breakpoints.size(); i++)
					{
						if (breakpoints[i].Addr == cmd.Arg0)
						{
							breakpoints[i].Enabled = cmd.Arg1 != 0;
							found = true;
							break;
						}
					}

					if (!found)
					{
						breakpoints.push_back(Breakpoint(cmd.Arg0));
					}
				}

				break;
			}

			case CommandType::ReadBreakpoint:
			case CommandType::WriteBreakpoint:
			{
				SCOPE_LOCK_MTX();

				bool is_read = cmd.Type == CommandType::ReadBreakpoint;
				vector<RWBreakpoint>& bplist = is_read ? core.GetMMU()->ReadBreakpoints() : core.GetMMU()->WriteBreakpoints();

				bool print_list = false;
				if (cmd.StrArg0.empty() && cmd.StrArg1.empty())
				{
					if (bplist.size() == 0)
					{
						console.PrintLn("No breakpoints set");
					}
					else
					{
						print_list = true;
					}
				}
				else if (!cmd.StrArg0.empty() && cmd.StrArg1.empty())
				{
					bplist.push_back(RWBreakpoint(cmd.Arg0, 0, false, false));
					print_list = true;
				}
				else if (!cmd.StrArg0.empty() && !cmd.StrArg1.empty())
				{
					uint16_t addr;

					if (StringEquals(cmd.StrArg1, "-") || StringEquals(cmd.StrArg1, "del"))
					{
					}
					else
					{
						bool is_mask = StringEndsWith(cmd.StrArg1, "m");

						if (is_mask)
							addr = ParseNumericString(cmd.StrArg1.substr(0, cmd.StrArg1.size() - 1));
						else
							addr = cmd.Arg1;
						bplist.push_back(RWBreakpoint(cmd.Arg0, addr, true, is_mask));
					}

					print_list = true;
				}

				if (print_list)
				{
					console.PrintLn(is_read ? " MMU Read Breakpoints" : " MMU Write Breakpoints");
					for (int i = 0; i < bplist.size(); i++)
					{
						auto& bp = bplist[i];
						if (bp.CheckValue && !bp.ValueIsMask)
							console.PrintLn(" [%d] %04Xh %02Xh", i, bp.Addr, bp.Value);
						else if (bp.CheckValue && bp.ValueIsMask)
							console.PrintLn(" [%d] %04Xh %02Xh (mask)", i, bp.Addr, bp.Value);
						else
							console.PrintLn(" [%d] %04Xh", i, bp.Addr);
					}
				}

				break;
			}

			case CommandType::Step:
			case CommandType::StepN:
			case CommandType::StepUntilVBlank:
			{
				if (paused.load() == true)
				{
					SCOPE_LOCK_MTX();

					if (cmd.Type == CommandType::StepUntilVBlank)
						thstate.StepParams.UntilVBlank = true;
					else
						thstate.StepParams.NSteps = cmd.Type == CommandType::Step ? 1 : cmd.Arg0;

					thstate.StepType = cmd.Type;
					thstate.Changed = true;
				}
				else
				{
					console.PrintLn("Emulation must be paused first");
				}

				break;
			}

			case CommandType::MemDump:
			{
				SCOPE_LOCK_MTX();
				console.MemDump(*core.GetMMU(), cmd.Arg0, cmd.Arg1);
				break;
			}

			case CommandType::Screenshot:
			{
				filesystem::path save_path;

				if (!cmd.StrArg0.empty())
				{
					cmd.StrArg0 = cmd.StrArg0.append(".bmp");
					if (filesystem::exists(cmd.StrArg0))
					{
						LOG_CONS("File already exists");
						break;
					}
					
					save_path = fs::absolute(fs::path(cmd.StrArg0));
				}
				else
				{
					long id = UnixTimestamp();
					string file_name("screenshot-");
					file_name.append(to_string(id));
					file_name.append(".bmp");
					save_path = fs::absolute(fs::path(file_name));
				}

				if (!fs::exists(save_path.parent_path()))
				{
					fs::create_directory(save_path.parent_path());
				}

				if (SDL_Surface* surface = SDL_CreateRGBSurface(0, GPU::LCDWidth, GPU::LCDHeight, 32, 0, 0, 0, 0))
				{
					SCOPE_LOCK_MTX();

					const ColourBuffer& frame = core.GetGPU()->GetFrameBuffer();
					uint8_t* pixels = (uint8_t*)surface->pixels;
					int index;

					for (int i = 0; i < frame.Count(); i++)
					{
						index = i * BYTES_PER_PIXEL;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
						pixels[index + 0] = frame[i].Alpha;
						pixels[index + 1] = frame[i].Red;
						pixels[index + 2] = frame[i].Green;
						pixels[index + 3] = frame[i].Blue;
#else
						pixels[index + 0] = frame[i].Blue;
						pixels[index + 1] = frame[i].Green;
						pixels[index + 2] = frame[i].Red;
						pixels[index + 3] = frame[i].Alpha;
#endif
					}

					auto s = save_path.string();
					const char* fullpath = s.c_str();

					if (SDL_RWops* rwops = SDL_RWFromFile(fullpath, "wb"))
					{
						if (SDL_SaveBMP_RW(surface, rwops, 1) == 0)
						{
							console.PrintLn("Saved screenshot to %s", fullpath);
						}
						else
						{
							console.PrintLn("Failed to save screenshot bitmap. Error: %s", SDL_GetError());
						}
					}
					else
					{
						console.PrintLn("Failed to save screenshot bitmap. Error: %s", SDL_GetError());
					}

					SDL_FreeSurface(surface);
				}

				break;
			}

			case CommandType::None: 
			default:
				LOG_CONS("Unknown command or invalid arguments");
				break;
		}
	}
}

void GemApp::WindowLoop()
{
	main_window = new GemWindow("", GPU::LCDWidth, GPU::LCDHeight, &sound, settings.VSync, settings.ResolutionScale);
	windows.push_back(main_window);

	SetWindowTitles();

	while (main_window->IsOpen())
	{
		ProcessEvents();
		
		if (!LoopWork())
			break;
	}

	sound.Pause();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	exit(0);
}

bool GemApp::LoopWork()
{
	SCOPE_LOCK_MTX();

	if (thstate.Changed)
	{
		if (thstate.Shutdown)
		{
			thstate.WindowLoopFinished = true;
			return false;
		}

		if (thstate.Reset)
		{
			if (!thstate.ROMPath.empty())
			{
				rom_file = thstate.ROMPath;
				thstate.ROMPath.clear();
			}

			if (InitCore())
			{
				// echo a new prompt because InitCore() prints some stuff out
				GemConsole::PrintPrompt();
				SetWindowTitles();
			}

			thstate.Reset = false;
		}

		OpenCloseWindows();

		if (thstate.FrameRateLimit != 0 && main_window->GetFrameRateLimit() != thstate.FrameRateLimit)
		{
			main_window->SetFrameRateLimit(thstate.FrameRateLimit);
			thstate.FrameRateLimit = 0;
		}

		main_window->ShowFPSCounter(thstate.ShowFPS);

		thstate.Changed = false;
	}
	else
	{
		HandleDropEventFileChanged();
	}

	bool emu_paused = paused.load();
	bool vblank;

	if (!emu_paused)
	{
		if (!settings.NoSound && !sound.IsPlaying())
			sound.Play();

		// Emulate the core for 1 frame, handling breakpoints if needed
		if (breakpoints.size() == 0 && !core.GetMMU()->AnyBreakpoints())
		{
			core.TickUntilVBlank();
			PresentWindows();
		}
		else
		{
			if (DebuggerTick(false))
				PresentWindows();
		}
	}
	else if (thstate.StepType != CommandType::None)
	{
		if (DebuggerTick(true))
			PresentWindows();
	}

	if (emu_paused)
		this_thread::sleep_for(30ms);

	return true;
}

void GemApp::PresentWindows()
{
	main_window->DrawFrame(core.GetGPU()->GetFrameBuffer());
	main_window->Present();

	if (tiles_window != nullptr && tiles_window->IsOpen())
	{
		core.GetGPU()->ComputeTileViewBuffer();
		tiles_window->DrawFrame(core.GetGPU()->GetTileViewBuffer());
		tiles_window->Present();
	}

	if (sprites_window != nullptr && sprites_window->IsOpen())
	{
		core.GetGPU()->DrawSpritesViewBuffer(*sprites_window);
		sprites_window->Present();
	}

	if (palettes_window != nullptr && palettes_window->IsOpen())
	{
		core.GetGPU()->DrawPaletteViewBuffer(*palettes_window);
		palettes_window->Present();
	}
}

bool GemApp::DebuggerTick(bool emu_paused)
{
	bool hit = false, vblank = false;
	bool stepping_finished = !emu_paused;
	int bp_index = -1, rbp_index = -1, wbp_index = -1;
	const vector<RWBreakpoint>& rbps = core.GetMMU()->ReadBreakpoints();
	const vector<RWBreakpoint>& wbps = core.GetMMU()->WriteBreakpoints();
	core.GetMMU()->EvalBreakpoints(true);

	while ((!emu_paused || !stepping_finished) && !hit && !vblank)
	{
		vblank = core.Tick();

		for (int i = 0; i < breakpoints.size(); i++)
		{
			if (breakpoints[i].Enabled && breakpoints[i].Addr == core.GetCPU().GetPC())
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

		if (emu_paused && thstate.StepType != CommandType::None)
		{
			switch (thstate.StepType)
			{
				case CommandType::Step:
				case CommandType::StepN:
				{
					int& cnt = thstate.StepParams.NSteps;
					if (--cnt == 0)
					{
						thstate.StepType = CommandType::None;
						stepping_finished = true;
					}

					break;
				}
				case CommandType::StepUntilVBlank:
				{
					if (vblank)
					{
						thstate.StepType = CommandType::None;
						stepping_finished = true;
					}

					break;
				}
			}
		}
	}

	if (bp_index >= 0)
	{
		paused.store(true);
		console.PrintLn("Breakpoint hit: %04Xh", breakpoints[bp_index].Addr);
		console.PrintPrompt();
	}
	else if (wbp_index >= 0)
	{
		paused.store(true);
		const RWBreakpoint& bp = wbps[wbp_index];

		if (bp.CheckValue)
			console.PrintLn("MMU Write Breakpoint hit: %04Xh %02Xh", bp.Addr, bp.Value);
		else
			console.PrintLn("MMU Write Breakpoint hit: %04Xh", bp.Addr);

		console.PrintPrompt();
	}
	else if (rbp_index >= 0)
	{
		paused.store(true);
		const RWBreakpoint& bp = rbps[rbp_index];

		if (bp.CheckValue)
			console.PrintLn("MMU Read Breakpoint hit: %04Xh %02Xh", bp.Addr, bp.Value);
		else
			console.PrintLn("MMU Read Breakpoint hit: %04Xh", bp.Addr);

		console.PrintPrompt();
	}
	else if (emu_paused && stepping_finished)
	{
		console.PrintCPUInfo(core);
		console.PrintPrompt();
		paused.store(true);
	}

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
				WindowEventHandler(event.window);
				break;
			case SDL_DROPFILE:
				DropEventHandler(event.drop);
				break;
			case SDL_KEYDOWN:
				KeyPressHandler(event.key);
				break;
			case SDL_KEYUP:
				KeyReleaseHandler(event.key);
				break;
		}
	}
}

void GemApp::WindowEventHandler(SDL_WindowEvent& ev)
{
	if (ev.event == SDL_WINDOWEVENT_CLOSE)
	{
		for (GemWindow* window : windows)
		{
			if (window->Id() == ev.windowID)
				window->Close();
		}
	}
}

void GemApp::DropEventHandler(SDL_DropEvent& ev)
{
	if (ev.windowID == main_window->Id() && filesystem::exists(ev.file))
	{
		drop_event_file = string(ev.file);
	}
}

void GemApp::KeyPressHandler(SDL_KeyboardEvent& ev)
{
	switch (ev.keysym.sym)
	{
		case SDLK_UP:
			core.GetJoypad()->Press(JoypadKey::Up);
			break;
		case SDLK_DOWN:
			core.GetJoypad()->Press(JoypadKey::Down);
			break;
		case SDLK_LEFT:
			core.GetJoypad()->Press(JoypadKey::Left);
			break;
		case SDLK_RIGHT:
			core.GetJoypad()->Press(JoypadKey::Right);
			break;
		case SDLK_SPACE:
			core.GetJoypad()->Press(JoypadKey::A);
			break;
		case SDLK_LSHIFT:
			core.GetJoypad()->Press(JoypadKey::B);
			break;
		case SDLK_RETURN:
			core.GetJoypad()->Press(JoypadKey::Start);
			break;
		case SDLK_BACKSLASH:
			core.GetJoypad()->Press(JoypadKey::Select);
			break;
	}
}

void GemApp::KeyReleaseHandler(SDL_KeyboardEvent& ev)
{
	switch (ev.keysym.sym)
	{
		case SDLK_UP:
			core.GetJoypad()->Release(JoypadKey::Up);
			break;
		case SDLK_DOWN:
			core.GetJoypad()->Release(JoypadKey::Down);
			break;
		case SDLK_LEFT:
			core.GetJoypad()->Release(JoypadKey::Left);
			break;
		case SDLK_RIGHT:
			core.GetJoypad()->Release(JoypadKey::Right);
			break;
		case SDLK_SPACE:
			core.GetJoypad()->Release(JoypadKey::A);
			break;
		case SDLK_LSHIFT:
			core.GetJoypad()->Release(JoypadKey::B);
			break;
		case SDLK_RETURN:
			core.GetJoypad()->Release(JoypadKey::Start);
			break;
		case SDLK_BACKSLASH:
			core.GetJoypad()->Release(JoypadKey::Select);
			break;
	}
}

void GemApp::OpenCloseWindows()
{
	if (thstate.ShowTilesWindow)
	{
		if (tiles_window == nullptr)
		{
			tiles_window = new GemWindow("Tiles", GPU::TileViewWidth, GPU::TileViewHeight, nullptr, settings.VSync, settings.ResolutionScale);
			windows.push_back(tiles_window);
		}
		else if (!tiles_window->IsOpen())
		{
			tiles_window->Open();
		}
	}
	else if (tiles_window != nullptr && tiles_window->IsOpen())
	{
		tiles_window->Close();
	}

	if (thstate.ShowPalettesWindow)
	{
		if (palettes_window == nullptr)
		{
			palettes_window = new GemWindow("Palettes", GPU::PalettesViewWidth, GPU::PalettesViewHeight, nullptr, settings.VSync, settings.ResolutionScale);
			windows.push_back(palettes_window);
		}
		else if (!palettes_window->IsOpen())
		{
			palettes_window->Open();
		}
	}
	else if (palettes_window != nullptr && palettes_window->IsOpen())
	{
		palettes_window->Close();
	}

	if (thstate.ShowSpritesWindow)
	{
		if (sprites_window == nullptr)
		{
			sprites_window = new GemWindow("Sprites", GPU::SpritesViewWidth, GPU::SpritesViewHeight, nullptr, settings.VSync, settings.ResolutionScale);
			windows.push_back(sprites_window);
		}
		else if (!sprites_window->IsOpen())
		{
			sprites_window->Open();
		}
	}
	else if (sprites_window != nullptr && sprites_window->IsOpen())
	{
		sprites_window->Close();
	}
}

void GemApp::HandleDropEventFileChanged()
{
	if (!drop_event_file.empty())
	{
		rom_file = drop_event_file;
		drop_event_file.clear();

		if (InitCore())
		{
			GemConsole::PrintPrompt();
			SetWindowTitles();
		}
	}
}