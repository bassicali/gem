
#include <exception>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <filesystem>

#include <windows.h>

#include <SDL.h>
#include <glad/glad.h>
#include "OpenGL/Common.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "FontAwesome.h"

#include "Logging.h"
#include "Util.h"
#include "MsgPad.h"
#include "GemDebugger.h"

using namespace std;
using namespace GemUtil;

ImFont* UIFont = nullptr;
ImFont* MonoFont = nullptr;
ImFont* IconFont = nullptr;

GemDebugger::GemDebugger()
	: core(nullptr)
	, window(nullptr)
	, windowId(0)
	, currentDisassemblyChunk(nullptr)
	, glContext(nullptr)
	, initialized(false)
	, hidden(false)
	, running(false)
	, RefreshModel(false)
{
}

void ConsoleHandler(Command& command, GemConsole& console, void* data)
{
	GemDebugger* debugger = (GemDebugger*) data;
	debugger->HandleConsoleCommand(command, console);
}

bool GemDebugger::Init()
{
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow("Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WND_WIDTH, WND_HEIGHT, flags);
	if (window == nullptr)
		throw exception("Failed to create window");

	windowId = SDL_GetWindowID(window);

	glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr)
		throw exception("Failed to create window");

	SDL_GL_MakeCurrent(window, glContext);
	SDL_GL_SetSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)__GetOpenGLAddr))
	{
		LOG_ERROR("Failed to initialize GL loader");
		return false;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends
	if (!ImGui_ImplSDL2_InitForOpenGL(window, glContext))
	{
		LOG_ERROR("Failed to intialize debugger; ImGui_ImplSDL2_InitForOpenGL did not succeed");
		return false;
	}

	if (!ImGui_ImplOpenGL3_Init(glsl_version))
	{
		LOG_ERROR("Failed to intialize debugger; ImGui_ImplOpenGL3_Init did not succeed");
		return false;
	}

	MonoFont = io.Fonts->AddFontDefault();
	UIFont = io.Fonts->AddFontFromFileTTF("DroidSans.ttf", 13);
	//IconFont = io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 13);

	if (!io.Fonts->Build())
		LOG_WARN("ImGui font atlas couldn't be built");


	for (int i = 0; i < GPU::NumSprites; i++)
	{
		spriteBuffers[i] = ColourBuffer(8, 16);
	}

	for (int i = 0; i < GPU::NumTilesPerSet; i++)
	{
		tileBuffers[i] = ColourBuffer(8, 8);
	}

	for (int i = 0; i < GPU::NumPaletteColours; i++)
	{
		paletteBuffers[i] = ColourBuffer(2, 2);
	}

	GemConsole::Get().SetHandler(ConsoleHandler);
	GemConsole::Get().SetHandlerUserData(this);

	initialized = true;
	running = true;

	return initialized;
}

void GemDebugger::HandleDisassembly(bool emu_paused)
{
	uint16_t pc = core->GetCPU().GetPC();

	DisassemblyMsgPad& dmsgpad = GMsgPad.Disassemble;

	if (dmsgpad.State == DisassemblyRequestState::Requested)
	{
		uint16_t start_addr = dmsgpad.UseCurrentPC ? pc : dmsgpad.Address;
		UpdateDisassembly(start_addr);

		dmsgpad.Address = start_addr;
		dmsgpad.State = DisassemblyRequestState::Ready;
		LOG_DEBUG("Sent update event after disassembling by request");
	}
	else if (emu_paused && dmsgpad.Address != pc)
	{
		int next_idx = dmsgpad.CurrentIndex + 1;
		if (next_idx < dmsgpad.CurrentChunk->Size())
		{
			uint16_t chunk_next_addr = dmsgpad.CurrentChunk->Entries[next_idx].Address;
			if (chunk_next_addr == pc)
			{
				dmsgpad.CurrentIndex = next_idx;
			}
			else
			{
				UpdateDisassembly(pc);
				LOG_DEBUG("Sent update event after disassembling because PC jumped");
			}
		}
		else
		{
			UpdateDisassembly(pc);
			LOG_DEBUG("Sent update event after disassembling because end of chunk was reached");
		}

		dmsgpad.Address = pc;
	}
}

void GemDebugger::Draw()
{
	if (!running)
		return;

	SDL_GL_MakeCurrent(window, glContext);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	
	ImGui::NewFrame();
	LayoutWidgets();

	ImGui::Render();

	ImGuiIO& io = ImGui::GetIO(); // Doing this for an assert
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
}

void SelectableColor(ImU32 color)
{
	ImVec2 p_min = ImGui::GetItemRectMin();
	ImVec2 p_max = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, color);
}

void GemDebugger::LayoutWidgets()
{
	static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;

	int ww, wh;
	SDL_GetWindowSize(window, &ww, &wh);

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	auto work_pos = viewport->WorkPos;
	auto work_size = viewport->WorkSize;

	int main_area_height = work_size.y - ImGui::GetTextLineHeightWithSpacing() * 2;
	int status_bar_height = work_size.y - main_area_height;

	ImGui::SetNextWindowPos(work_pos);
	ImGui::SetNextWindowSize(ImVec2(work_size.x, main_area_height));
	
	static bool is_paused;
	is_paused = GMsgPad.EmulationPaused.load();

	static bool update_cpu_registers, update_lcd_registers;
	update_cpu_registers = false;
	update_lcd_registers = false;

	// While paused, don't refresh the register strings so the user can edit them
	bool refresh_registers = !is_paused || RefreshModel;
	model.SetValuesFromCore(*core, refresh_registers, true, refresh_registers);
	RefreshModel = false;

	ImGui::PushFont(UIFont);
	if (ImGui::Begin("Debugger_MainArea", nullptr, flags))
	{
		ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
		if (ImGui::BeginTabBar("MainTabBar", tab_bar_flags))
		{
			if (ImGui::BeginTabItem("CPU"))
			{
				if (ImGui::BeginTable("CPUTabSplit", 2, ImGuiTableFlags_None))
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::BeginListBox("", ImVec2(ImGui::GetWindowWidth() / 2, ImGui::GetWindowHeight() - 41.0)))
					{
						ImGui::PushFont(MonoFont);

						DisassemblyMsgPad& dmsgpad = GMsgPad.Disassemble;
						if (dmsgpad.CurrentChunk)
						{
							ImDrawList* draw_list = ImGui::GetWindowDrawList();

							if (disassemblyTextList.size() == 0 || currentDisassemblyChunk != dmsgpad.CurrentChunk)
							{
								currentDisassemblyChunk = dmsgpad.CurrentChunk;
								GenerateDisassemblyText(*currentDisassemblyChunk, disassemblyTextList);
							}

							for (int idx = 0; idx < disassemblyTextList.size(); idx++)
							{
								draw_list->ChannelsSplit(2);
								draw_list->ChannelsSetCurrent(1);

								bool is_selected = idx == dmsgpad.CurrentIndex;
								DisassemblyEntry& entry = currentDisassemblyChunk->Entries[idx];
								ImGui::Selectable(disassemblyTextList[idx].c_str(), is_selected);

								if (entry.Breakpoint)
								{
									if (!ImGui::IsItemHovered() && !is_selected)
									{
										draw_list->ChannelsSetCurrent(0);
										SelectableColor(IM_COL32(230, 0, 0, 200));
									}
									else if (is_selected)
									{
										draw_list->ChannelsSetCurrent(0);
										SelectableColor(IM_COL32(155, 0, 0, 200));
									}
								}

								draw_list->ChannelsMerge();

								if (is_selected)
									ImGui::SetItemDefaultFocus();

								if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
								{
									bool* bp = &currentDisassemblyChunk->Entries[idx].Breakpoint;
									*bp = !*bp;

									if (*bp)
									{
										breakpoints.push_back(Breakpoint(entry.Address));
									}
									else
									{
										for (int i = 0; i < breakpoints.size(); i++)
										{
											if (breakpoints[i].Addr == entry.Address)
											{
												breakpoints.erase(breakpoints.begin() + i);
												break;
											}
										}
									}
								}
							}

						}

						ImGui::PopFont();
						ImGui::EndListBox();
					}

					ImGui::TableSetColumnIndex(1);
					
					ImGui::Text("Registers");
					if (ImGui::BeginTable("RegisterTextboxes", 2, ImGuiTableFlags_None))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("PC", model.Reg_PC, 5);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("SP", model.Reg_SP, 5);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("A", model.Reg_A, 3);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("F", model.Reg_F, 3);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("B", model.Reg_B, 3);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("C", model.Reg_C, 3);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("D", model.Reg_D, 3);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("E", model.Reg_E, 3);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("H", model.Reg_H, 3);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("L", model.Reg_L, 3);

						ImGui::EndTable();
					}

					if (ImGui::Button("Update##cpu"))
						update_cpu_registers = true;

					ImGui::Separator();

					ImGui::Text("Interrupts");

					ImGui::Checkbox("Master Enable", &model.IME);

					if (ImGui::BeginTable("InterruptCheckboxes", 3, ImGuiTableFlags_None))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("Enabled");
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("Requested");

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("V-Blank");
						ImGui::TableSetColumnIndex(1);
						ImGui::Checkbox("##VBlankEnabled", &model.VBlankEnabled);
						ImGui::TableSetColumnIndex(2);
						ImGui::Checkbox("##VBlankRequested", &model.VBlankRequested);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("LCD");
						ImGui::TableSetColumnIndex(1);
						ImGui::Checkbox("##LCDEnabled", &model.LCDEnabled);
						ImGui::TableSetColumnIndex(2);
						ImGui::Checkbox("##LCDRequested", &model.LCDRequested);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Timer");
						ImGui::TableSetColumnIndex(1);
						ImGui::Checkbox("##TimerEnabled", &model.TimerEnabled);
						ImGui::TableSetColumnIndex(2);
						ImGui::Checkbox("##TimerRequested", &model.TimerRequested);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Serial");
						ImGui::TableSetColumnIndex(1);
						ImGui::Checkbox("##SerialEnabled", &model.SerialEnabled);
						ImGui::TableSetColumnIndex(2);
						ImGui::Checkbox("##SerialRequested", &model.SerialRequested);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Joypad");
						ImGui::TableSetColumnIndex(1);
						ImGui::Checkbox("##JoypadEnabled", &model.JoypadEnabled);
						ImGui::TableSetColumnIndex(2);
						ImGui::Checkbox("##JoypadRequested", &model.JoypadRequested);

						ImGui::EndTable();
					}
					
					ImGui::Separator();

					ImGui::Text("Timers");
					if (ImGui::BeginTable("TimerInfoTable", 3, ImGuiTableFlags_None))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("Frequency");

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("DIV");
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", core->GetMMU()->GetTimerController().Divider);
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("16384 Hz", core->GetMMU()->GetTimerController().Divider);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("TIMA");
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", core->GetMMU()->GetTimerController().Counter);
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%d Hz", core->GetMMU()->GetTimerController().GetCounterFrequency());

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("TMA");
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", core->GetMMU()->GetTimerController().Modulo);
						
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Frames");
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", core->GetFrameCount());

						ImGui::EndTable();
					}

					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Graphics"))
			{
				if (ImGui::BeginTable("GPUTabSplit", 2, ImGuiTableFlags_None))
				{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					LayoutGPUVisuals();

					ImGui::TableSetColumnIndex(1);

					ImGui::Text("LCD Control (%02Xh)", core->GetGPU()->GetLCDControl().ReadByte());
					if (ImGui::BeginTable("LCDCTextboxes", 2, ImGuiTableFlags_None))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Enabled", &model.LCDC_Enabled);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Window Map", &model.LCDC_WinMap);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Window Enabled", &model.LCDC_WinEnabled);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Tile Select", &model.LCDC_TileDataSelect);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Background Map", &model.LCDC_BGMap);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Sprite Size", &model.LCDC_SpriteSize);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Sprite Enabled", &model.LCDC_SpriteEnabled);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("Background Enabled", &model.LCDC_BGEnabled);

						ImGui::EndTable();
					}

					ImGui::NewLine();

					ImGui::Text("LCD Status (%02Xh)", core->GetGPU()->GetLCDStatus().ReadByte());
					if (ImGui::BeginTable("LCDStatTextboxes", 2, ImGuiTableFlags_None))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("LYC Interrupt", &model.LCDS_LYCInt);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("OAM Interrupt", &model.LCDS_OAMInt);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("VB Interrupt", &model.LCDS_VBInt);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("HB Interrupt", &model.LCDS_HBInt);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::Checkbox("LYC=LY", &model.LCDS_LYEqLYC);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("LY", model.LCDS_LY, 2);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("Mode", model.LCDS_Mode, 2);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						ImGui::InputText("LYC", model.LCDS_LYC, 2);

						ImGui::EndTable();
					}

					ImGui::NewLine();

					if (ImGui::Button("Update##lcds"))
						update_lcd_registers = true;

					ImGui::NewLine();
					ImGui::Separator();
					ImGui::NewLine();

					static bool wash_out_colours = true;
					ImGui::Checkbox("Wash-out Colours", &wash_out_colours);
					core->GetGPU()->SetColourCorrectionMode(wash_out_colours ? CorrectionMode::Washout : CorrectionMode::Scale);

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sound"))
			{
				if (ImGui::BeginTable("SoundTabHSplit", 1, ImGuiTableFlags_None))
				{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					LayoutAudioVisuals();

					ImGui::Separator();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);

					ImGui::TableNextColumn();

					ImGui::Text("Controls");
					if (ImGui::BeginTable("APUControls", 2, ImGuiTableFlags_None))
					{
						ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::SetNextItemWidth(40);
						static bool apu_mute = false;
						ImGui::Checkbox("Mute", &apu_mute);
						core->GetAPU()->SetMuted(apu_mute);

						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						static bool apu_chan1 = true;
						ImGui::Checkbox("Channel 1", &apu_chan1);
						core->GetAPU()->SetChannelMask(0, apu_chan1);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						static bool apu_chan2 = true;
						ImGui::Checkbox("Channel 2", &apu_chan2);
						core->GetAPU()->SetChannelMask(1, apu_chan2);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						static bool apu_chan3 = true;
						ImGui::Checkbox("Channel 3", &apu_chan3);
						core->GetAPU()->SetChannelMask(0, apu_chan3);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
						ImGui::SetNextItemWidth(40);
						static bool apu_chan4 = true;
						ImGui::Checkbox("Channel 4", &apu_chan4);
						core->GetAPU()->SetChannelMask(3, apu_chan4);

						ImGui::EndTable();
					}

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Console"))
			{
				GemConsole::Get().Draw(MonoFont);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + main_area_height));
	ImGui::SetNextWindowSize(ImVec2(work_size.x, status_bar_height));

	if (ImGui::Begin("Debugger_StatusBarArea", nullptr, flags))
	{

		ImGui::PushFont(UIFont);
		if (ImGui::Button(is_paused ? "Play" : "Pause"))
		{
			is_paused = GMsgPad.TogglePause();
			if (is_paused)
			{
				GMsgPad.Disassemble.NumInstructions = 100;
				GMsgPad.Disassemble.Address = 0;
				GMsgPad.Disassemble.UseCurrentPC = true;
				GMsgPad.Disassemble.State = DisassemblyRequestState::Requested;
				LOG_DEBUG("Disassembly requested after Pause button was clicked");

				update_cpu_registers = true;
				update_lcd_registers = true;
			}
		}
		ImGui::PopFont();
		ImGui::SameLine();
		if (ImGui::Button("Step"))
		{
			if (is_paused)
			{
				GMsgPad.StepType = StepType::Step;
				GMsgPad.StepParams.NSteps = 1;
			}
		}
		// TODO
		//ImGui::SameLine();
		//if (ImGui::Button("Step Over"))
		//{
		//}
		ImGui::SameLine();
		if (ImGui::Button("Until V-Blank"))
		{
			if (is_paused)
				GMsgPad.StepType = StepType::StepUntilVBlank; // TODO: not working
		}

		ImGui::SameLine();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y));
		if (ImGui::Button("Open..."))
		{
			string path = GemUtil::PromptForROM();
			if (path.length() > 0)
			{
				GMsgPad.ROMPath = path;
				GMsgPad.Reset = true;
			}
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			GMsgPad.Reset = true;
		}

		ImGui::Text("FPS: %.1fHz", ImGui::GetIO().Framerate);

		ImGui::End();
	}


	ImGui::PopFont();

	if (is_paused)
		model.UpdateCore(*core, update_cpu_registers, true, update_lcd_registers);
}

void GemDebugger::LayoutGPUVisuals()
{
	static ImVec2 uv_min = ImVec2(0.0f, 1.0f);                 // Top-left
	static ImVec2 uv_max = ImVec2(1.0f, 0.0f);                 // Lower-right
	static ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
	static ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
	static int scale = 3;

	static int viz_option = 0;
	ImGui::RadioButton("Tiles", &viz_option, 0); ImGui::SameLine();
	ImGui::RadioButton("Sprites", &viz_option, 1); ImGui::SameLine();
	ImGui::RadioButton("Palettes", &viz_option, 2);

	if (viz_option == 0)
	{
		core->GetGPU()->RenderTilesViz(tileBuffers, tileAttrs);

		if (ImGui::BeginTable("TilesTable", 8, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

			for (int row = 0; row < 16; row++)
			{
				ImGui::TableNextRow();

				for (int col = 0; col < 8; col++)
				{
					ImGui::TableNextColumn();
					int idx = row * 8 + col;

					auto& uploader = tilePxUploaders[idx];
					if (!uploader.IsInitialized())
					{
						if (!uploader.Init(&tileBuffers[idx]))
						{
							assert(false);
						}
					}

					uploader.Upload();
					ImGui::Image((ImTextureID)(intptr_t)uploader.TextureId(), ImVec2(8 * scale, 8 * scale), uv_min, uv_max, tint_col, border_col);

					if (ImGui::IsItemHovered())
					{
						auto& attr = tileAttrs[idx];

						ImGui::BeginTooltip();
						ImGui::PushFont(MonoFont);
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::Text("Bank: %d", attr.VRamBank);
						ImGui::Text("Palette: %d", attr.Palette);
						ImGui::Text("Priority: %d", attr.PriorityOverSprites);
						ImGui::Text("HFlip: %d", attr.HorizontalFlip);
						ImGui::Text("VFlip: %d", attr.VerticalFlip);
						ImGui::PopTextWrapPos();
						ImGui::PopFont();
						ImGui::EndTooltip();
					}
				}
			}

			ImGui::EndTable();
		}
	}
	else if (viz_option == 1)
	{
		core->GetGPU()->RenderSpritesViz(spriteBuffers, spriteInfos);

		if (ImGui::BeginTable("SpritesTable", 5, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

			for (int row = 0; row < 8; row++)
			{
				ImGui::TableNextRow();

				for (int col = 0; col < 5; col++)
				{
					ImGui::TableNextColumn();

					int idx = row * 5 + col;

					auto& uploader = spritePxUploaders[idx];
					if (!uploader.IsInitialized())
					{
						if (!uploader.Init(&spriteBuffers[idx]))
						{
							assert(false);
						}
					}

					uploader.Upload();
					ImGui::Image((ImTextureID)(intptr_t)uploader.TextureId(), ImVec2(8 * scale, 16 * scale), uv_min, uv_max, tint_col, border_col);

					if (ImGui::IsItemHovered())
					{
						auto& sprite = spriteInfos[idx];

						ImGui::BeginTooltip();
						ImGui::PushFont(MonoFont);
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::Text("(%3d,%3d)", sprite.XPos, sprite.YPos);
						ImGui::Text("Tile: %02Xh", sprite.Tile);
						ImGui::Text("HFlip: %d", sprite.HorizontalFlip);
						ImGui::Text("VFlip: %d", sprite.VerticalFlip);
						ImGui::PopTextWrapPos();
						ImGui::PopFont();
						ImGui::EndTooltip();
					}
				}
			}

			ImGui::EndTable();
		}
	}
	else if (viz_option == 2)
	{
		core->GetGPU()->RenderPalettesViz(paletteBuffers, paletteEntries);

		if (ImGui::BeginTable("PaletteTable", 4, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

			for (int row = 0; row < 16; row++)
			{
				ImGui::TableNextRow();

				for (int col = 0; col < 4; col++)
				{
					ImGui::TableNextColumn();

					int idx = row * 4 + col;
					auto& uploader = palettePxUploaders[idx];
					if (!uploader.IsInitialized())
					{
						if (!uploader.Init(&paletteBuffers[idx]))
						{
							assert(false);
						}
					}

					uploader.Upload();
					ImGui::Image((ImTextureID)(intptr_t)uploader.TextureId(), ImVec2(8 * scale, 8 * scale), uv_min, uv_max, tint_col, border_col);

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushFont(MonoFont);
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::Text("%s", row < 8 ? "Background" : "Sprites");
						ImGui::Text("%04Xh", paletteEntries[idx].AsWord());
						ImGui::PopTextWrapPos();
						ImGui::PopFont();
						ImGui::EndTooltip();
					}
				}
			}

			ImGui::EndTable();
		}
	}
}

void GemDebugger::LayoutAudioVisuals()
{
	audioSnapshot = core->GetAPU()->Snapshot();

	const ImU32 colr_right = ImGui::GetColorU32(IM_COL32(0, 0, 255, 255));
	const ImU32 colr_left = ImGui::GetColorU32(IM_COL32(128, 0, 255, 255));

	float draw_area_width = ImGui::GetWindowWidth();
	float half_width = draw_area_width / 2;
	const float bar_height = 20;
	ImVec2 p0;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	if (ImGui::BeginTable("APULevels", 1))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Channel 1");

		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.L_Emitter1) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_left);
			ImGui::InvisibleButton("##ch1l", size);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.R_Emitter1) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_right);
			ImGui::InvisibleButton("##ch1r", size);
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Channel 2");

		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.L_Emitter2) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_left);
			ImGui::InvisibleButton("##ch2l", size);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.R_Emitter2) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_right);
			ImGui::InvisibleButton("##ch2r", size);
		}


		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Channel 3");

		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.L_Emitter3) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_left);
			ImGui::InvisibleButton("##ch3l", size);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.R_Emitter3) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_right);
			ImGui::InvisibleButton("##ch3r", size);
		}


		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Channel 4");

		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.L_Emitter4) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_left);
			ImGui::InvisibleButton("##ch4l", size);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			float ratio = float(audioSnapshot.R_Emitter4) / 255.0f;
			float bar_width = max(ratio * half_width, 1.0f);
			ImVec2 size = ImVec2(bar_width, bar_height);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			draw_list->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), colr_right);
			ImGui::InvisibleButton("##ch4r", size);
		}

		ImGui::EndTable();
	}

	ImGui::Separator();

	if (ImGui::BeginTable("Chan4Wave", 1, ImGuiTableFlags_None))
	{
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Chan. 4 Wave Pattern");

		for (int addr = 0, idx = 0; 
				addr < 16;
				addr++, idx += 2)
		{
			uint8_t data = core->GetAPU()->ReadWaveRAM(addr);
			chan4Wave[idx] = float(data & 0xF) / 16.0f;
			chan4Wave[idx + 1] = float((data & 0xF0) >> 4) / 16.0f;
		}

		ImGui::PlotLines("##Chan4WavePlot", chan4Wave, 32);

		ImGui::EndTable();
	}
}

void GemDebugger::HandleConsoleCommand(Command& cmd, GemConsole& console)
{
	namespace fs = std::filesystem;

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
					GMsgPad.ROMPath = fs::absolute(load_path).string();
					GMsgPad.Reset = true;
				}
			}
			else
			{
				string path = GemUtil::PromptForROM();
				if (path.length() > 0)
				{
					GMsgPad.ROMPath = path;
					GMsgPad.Reset = true;
				}
			}

			break;
		}

		case CommandType::Trace:
		{
			if (cmd.Arg0)
			{
				auto& filename = core->StartTrace();
				console.PrintLn("CPU trace started: %s", filename.c_str());
			}
			else
			{
				core->EndTrace();
				console.PrintLn("CPU trace stopped");
			}

			break;
		}
		case CommandType::Run:
		{
			GMsgPad.EmulationPaused.store(false);
			break;
		}
		case CommandType::Pause:
		{
			GMsgPad.EmulationPaused.store(true);
			break;
		}
		case CommandType::Save:
		{
			shared_ptr<CartridgeReader> cart = core->GetCartridgeReader();
			if (cart)
			{
				if (cart->Properties().ExtRamHasBattery)
				{
					core->GetMMU()->SaveGame();
					console.PrintLn("Save-game written to %s", core->GetCartridgeReader()->SaveGameFile().c_str());
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
			core->GetAPU()->SetChannelMask(0, 0);
			core->GetAPU()->SetChannelMask(1, 0);
			core->GetAPU()->SetChannelMask(2, 0);
			core->GetAPU()->SetChannelMask(3, 0);
			console.PrintLn("APU muted");
			break;
		}
		case CommandType::APUUnmute:
		{
			core->GetAPU()->SetChannelMask(0, 1);
			core->GetAPU()->SetChannelMask(1, 1);
			core->GetAPU()->SetChannelMask(2, 1);
			core->GetAPU()->SetChannelMask(3, 1);
			console.PrintLn("APU unmuted");
			break;
		}
		case CommandType::APUChannelMask:
		{
			int index = cmd.Arg0 - 1;
			if (index >= 0 && index <= 3 && (cmd.Arg1 == 0 || cmd.Arg1 == 1))
			{
				core->GetAPU()->SetChannelMask(index, cmd.Arg1);
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
			switch (cmd.Arg0)
			{
			case 0:
				core->GetGPU()->SetColourCorrectionMode(CorrectionMode::Washout);
				break;
			case 1:
				core->GetGPU()->SetColourCorrectionMode(CorrectionMode::Scale);
				break;
			}
			break;
		}
		case CommandType::Brightness:
		{
			core->GetGPU()->SetBrightness(cmd.FArg0);
			break;
		}
		case CommandType::Reset:
		{
			GMsgPad.Reset = true;
			break;
		}
		case CommandType::Exit:
		{
			// End the console loop and signal the window loop to also stop
			GMsgPad.Shutdown = true;
			break;
		}

		case CommandType::Breakpoint:
		{
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
			bool is_read = cmd.Type == CommandType::ReadBreakpoint;
			vector<RWBreakpoint>& bplist = is_read ? core->GetMMU()->ReadBreakpoints() : core->GetMMU()->WriteBreakpoints();

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
			if (GMsgPad.EmulationPaused.load() == true)
			{
				//if (cmd.Type == CommandType::StepUntilVBlank)
				//	GMsgPad.StepParams.UntilVBlank = true;
				//else
				GMsgPad.StepParams.NSteps = cmd.Type == CommandType::Step ? 1 : cmd.Arg0;
			}
			else
			{
				console.PrintLn("Emulation must be paused first");
			}

			break;
		}

		case CommandType::MemDump:
		{
			MemDump(cmd.Arg0, cmd.Arg1);
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
				const ColourBuffer& frame = core->GetGPU()->GetFrameBuffer();
				uint8_t* pixels = (uint8_t*)surface->pixels;
				int index;

				for (int i = 0; i < frame.Count(); i++)
				{
					index = i * 4;
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

void GemDebugger::GenerateDisassemblyText(const DisassemblyChunk& chunk, vector<string>& out_text)
{
	out_text.clear();

	stringstream ss;
	ss << hex << right;

	for (auto& entry : chunk.Entries)
	{
		string op_str = (uint16_t(entry.OpCode) & 0xCB00) == 0xCB00
						? StringFmt("CB %02X", int(uint16_t(entry.OpCode) & 0xFF))
						: StringFmt("%02X", entry.OpCode);

		string imm_str;
		if (entry.NumImmediates == 1)
			imm_str = StringFmt(" %02X", entry.Imm0);
		else if (entry.NumImmediates == 2)
			imm_str = StringFmt(" %02X %02X", entry.Imm0, entry.Imm1);

		string bytes_str = StringFmt("%s%s", op_str.c_str(), imm_str.c_str());
		string mnemonic = entry.GetMnemonic();
		string dasm_str = StringFmt("%04X | %8s | %s", entry.Address, bytes_str.c_str(), mnemonic.c_str());
		out_text.push_back(dasm_str);
	}

	assert(chunk.Size() == out_text.size());
}

void GemDebugger::HandleEvent(SDL_Event& event)
{
	if (!IsInitialized())
		return;

	ImGui_ImplSDL2_ProcessEvent(&event);

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
		running = false;
}

void GemDebugger::SetCore(Gem* ptr)
{
	core = ptr;
	model.SetValuesFromCore(*core);
}

void GemDebugger::UpdateDisassembly(uint16_t addr)
{
	DisassemblyMsgPad& dmsgpad = GMsgPad.Disassemble;
	DisassemblyChunk* chunk = dmsgpad.CurrentChunk;

	const int SLACK = 0;

	if (chunk && addr >= chunk->StartAddr() && addr <= (chunk->EndAddr() - SLACK))
	{
		// TODO: use a binary search here since Entries is sorted by address
		bool found = false;
		for (int idx = 0; idx < chunk->Size(); idx++)
		{
			if (chunk->Entries[idx].Address == addr)
			{
				dmsgpad.CurrentIndex = idx;
				found = true;
				break;
			}
		}

		// TODO: new data was probably written to this memory space and now the PC doesn't line up with a opcode
		assert(found);
	}
	else
	{
		int chunk_index = 0;
		chunk = nullptr;
		Disassembler::Decode(addr, dmsgpad.NumInstructions, false, *core->GetMMU(), dmsgpad.Output, chunk, chunk_index);
		dmsgpad.CurrentChunk = chunk;
		dmsgpad.CurrentIndex = chunk_index;
	}
}

void GemDebugger::Reset()
{
	audioSnapshot = EmittersSnapshot();
	memset(chan4Wave, 0, sizeof(float) * 32);
	model.SetValuesFromCore(*core);
}

void GemDebugger::MemDump(uint16_t start, int count)
{
	if (count % 16 != 0)
		count = count + 16 - count % 16;
	
	stringstream ss;

	ss << "MemDump - " << count << " bytes start at: " << hex << uppercase << setfill('0') << setw(4) << start << endl;

	uint16_t addr = (start / 16) * 16;
	ss << hex << uppercase << setfill('0') << right;
	int data;
	bool stop = false;

	string str_buff(16, ' ');

	for (int ln = 0; ln < (count / 16) && !stop; ln++)
	{
		ss << " " << setw(4) << addr << " | ";

		for (int i = 0; i < 16; i++, addr++)
		{
			if (addr >= 0xFF00)
			{
				stop = true;
				break;
			}

			data = core->GetMMU()->ReadByte(addr);
			str_buff[i] = data;

			if (addr == start)
				ss << setw(2) << data << " ";
			else
				ss << setw(2) << data << " ";
		}

		if (!stop)
			ss << "| " << SanitizeString(str_buff, '.');

		ss << endl;
	}
	
	GemConsole::Get().PrintLn(ss.str().c_str());
}

///////////////////////////
///    UIEditingModel   ///
///////////////////////////
UIEditingModel::UIEditingModel(Gem& core)
{
	SetValuesFromCore(core);
}

void UIEditingModel::SetValuesFromCore(Gem& core, bool registers, bool interrupt_info, bool lcd_registers, bool apu_registers)
{
	if (registers)
	{
		sprintf_s((char*)Reg_A, 3, "%02X", core.GetCPU().GetRegisterA());
		sprintf_s((char*)Reg_F, 3, "%02X", core.GetCPU().GetRegisterF());
		sprintf_s((char*)Reg_B, 3, "%02X", core.GetCPU().GetRegisterB());
		sprintf_s((char*)Reg_C, 3, "%02X", core.GetCPU().GetRegisterC());
		sprintf_s((char*)Reg_D, 3, "%02X", core.GetCPU().GetRegisterD());
		sprintf_s((char*)Reg_E, 3, "%02X", core.GetCPU().GetRegisterE());
		sprintf_s((char*)Reg_H, 3, "%02X", core.GetCPU().GetRegisterH());
		sprintf_s((char*)Reg_L, 3, "%02X", core.GetCPU().GetRegisterL());
		sprintf_s((char*)Reg_PC, 5, "%04X", core.GetCPU().GetPC());
		sprintf_s((char*)Reg_SP, 5, "%04X", core.GetCPU().GetSP());
	}

	if (interrupt_info)
	{
		IME = core.GetCPU().IsInterruptsEnabled();
		VBlankEnabled = core.GetMMU()->GetInterruptController()->VBlankEnabled;
		VBlankRequested = core.GetMMU()->GetInterruptController()->VBlankRequested;
		LCDEnabled = core.GetMMU()->GetInterruptController()->LCDStatusEnabled;
		LCDRequested = core.GetMMU()->GetInterruptController()->LCDStatusRequested;
		TimerEnabled = core.GetMMU()->GetInterruptController()->TimerEnabled;
		TimerRequested = core.GetMMU()->GetInterruptController()->TimerRequested;
		SerialEnabled = core.GetMMU()->GetInterruptController()->SerialEnabled;
		SerialRequested = core.GetMMU()->GetInterruptController()->SerialRequested;
		JoypadEnabled = core.GetMMU()->GetInterruptController()->JoypadEnabled;
		JoypadRequested = core.GetMMU()->GetInterruptController()->JoypadRequested;
	}

	if (lcd_registers)
	{
		auto& ctrl = core.GetGPU()->GetLCDControl();
		LCDC_Enabled = ctrl.Enabled;
		LCDC_WinMap = ctrl.WindowTileMapSelect;
		LCDC_WinEnabled = ctrl.WindowEnabled;
		LCDC_TileDataSelect = ctrl.BgWindowTileDataSelect;
		LCDC_BGMap = ctrl.BgTileMapSelect;
		LCDC_SpriteSize = ctrl.SpriteSize;
		LCDC_SpriteEnabled = ctrl.SpriteEnabled;
		LCDC_BGEnabled = ctrl.BgDisplay;

		auto& stat = core.GetGPU()->GetLCDStatus();
		LCDS_LYCInt = stat.LycLyCoincidenceIntEnabled;
		LCDS_OAMInt = stat.OamIntEnabled;
		LCDS_VBInt = stat.VBlankIntEnabled;
		LCDS_HBInt = stat.HBlankIntEnabled;
		LCDS_LYEqLYC = stat.LycLyCoincidence;

		sprintf_s((char*)LCDS_LY, 5, "%d", core.GetGPU()->GetLCDPositions().LineY);
		sprintf_s((char*)LCDS_LYC, 5, "%d", core.GetGPU()->GetLCDPositions().LineYCompare);
		sprintf_s((char*)LCDS_Mode, 2, "%d", core.GetGPU()->GetLCDStatus().Mode);
	}
}

void UIEditingModel::UpdateCore(Gem& core, bool registers, bool interrupt_info, bool lcd_registers, bool apu_registers)
{
	if (registers)
	{
		core.GetCPU().SetRegisterA(strtol(Reg_A, nullptr, 16));
		core.GetCPU().SetRegisterF(strtol(Reg_F, nullptr, 16));
		core.GetCPU().SetRegisterB(strtol(Reg_B, nullptr, 16));
		core.GetCPU().SetRegisterC(strtol(Reg_C, nullptr, 16));
		core.GetCPU().SetRegisterD(strtol(Reg_D, nullptr, 16));
		core.GetCPU().SetRegisterE(strtol(Reg_E, nullptr, 16));
		core.GetCPU().SetRegisterH(strtol(Reg_H, nullptr, 16));
		core.GetCPU().SetRegisterL(strtol(Reg_L, nullptr, 16));
		core.GetCPU().SetPC(strtol(Reg_PC, nullptr, 16));
		core.GetCPU().SetSP(strtol(Reg_SP, nullptr, 16));
	}
	
	if (interrupt_info)
	{
		core.GetCPU().SetInterruptsEnabled(IME);
		core.GetMMU()->GetInterruptController()->VBlankEnabled = VBlankEnabled;
		core.GetMMU()->GetInterruptController()->VBlankRequested = VBlankRequested;
		core.GetMMU()->GetInterruptController()->LCDStatusEnabled = LCDEnabled;
		core.GetMMU()->GetInterruptController()->LCDStatusRequested = LCDRequested;
		core.GetMMU()->GetInterruptController()->TimerEnabled = TimerEnabled;
		core.GetMMU()->GetInterruptController()->TimerRequested = TimerRequested;
		core.GetMMU()->GetInterruptController()->SerialEnabled = SerialEnabled;
		core.GetMMU()->GetInterruptController()->SerialRequested = SerialRequested;
		core.GetMMU()->GetInterruptController()->JoypadEnabled = JoypadEnabled;
		core.GetMMU()->GetInterruptController()->JoypadRequested = JoypadRequested;
	}

	if (lcd_registers)
	{
		auto& ctrl = core.GetGPU()->GetLCDControl();
		ctrl.Enabled = LCDC_Enabled;
		ctrl.WindowTileMapSelect = LCDC_WinMap;
		ctrl.WindowEnabled = LCDC_WinEnabled;
		ctrl.BgWindowTileDataSelect = LCDC_TileDataSelect;
		ctrl.BgTileMapSelect = LCDC_BGMap;
		ctrl.SpriteSize = LCDC_SpriteSize;
		ctrl.SpriteEnabled = LCDC_SpriteEnabled;
		ctrl.BgDisplay = LCDC_BGEnabled;

		auto& stat = core.GetGPU()->GetLCDStatus();
		LCDS_LYCInt = stat.LycLyCoincidenceIntEnabled;
		LCDS_OAMInt = stat.OamIntEnabled;
		LCDS_VBInt = stat.VBlankIntEnabled;
		LCDS_HBInt = stat.HBlankIntEnabled;
		LCDS_LYEqLYC = stat.LycLyCoincidence;

		int state = strtol(LCDS_Mode, nullptr, 16);
		if (state <= 3)
			stat.Mode = (LCDMode)state;
			
  		core.GetGPU()->GetLCDPositions().LineY = strtol(LCDS_LY, nullptr, 16);
  		core.GetGPU()->GetLCDPositions().LineYCompare = strtol(LCDS_LYC, nullptr, 16);
	}
}
