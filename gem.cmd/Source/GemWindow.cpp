#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <sstream>
#include <cmath>
#include <cassert>

#include <Windows.h>

#include "Core/GPU.h"
#include "GemWindow.h"

bool GemWindow::_bFirstInstance = true;
TTF_Font* GemWindow::mainFont = nullptr;

using namespace std;

GemWindow::GemWindow(const char* title, int buff_width, int buff_height, IAudioQueue* queue, bool vsync, float custom_scale /*= 1*/) :
	bufferWidth(buff_width),
	bufferHeight(buff_height),
	frameCtr(0),
	showFPS(false),
	avgFrameTime(0),
	vsync(vsync)
{
	if (custom_scale <= 0)
		throw exception("Invalid custom_scale");

	float dpi_aware_scale = GetDpiAwareScalingFactor();
	scaleFactor = custom_scale * dpi_aware_scale;

	windowWidth = static_cast<int>(ceil(scaleFactor * bufferWidth));
	windowHeight = static_cast<int>(ceil(scaleFactor * bufferHeight));

	window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
	if (window == nullptr)
		throw exception("Failed to create window");

	SDL_SetWindowResizable(window, SDL_TRUE);
	windowId = SDL_GetWindowID(window);

	Uint32 flags = SDL_RENDERER_ACCELERATED;
	if (vsync)
	{
		flags |= SDL_RENDERER_PRESENTVSYNC;
		SetFrameRateLimit(0);
	}
	else
	{
		SetFrameRateLimit(DefaultFrameRate);
	}

	renderer = SDL_CreateRenderer(window, -1, flags);
	if (renderer == nullptr)
		throw exception("Failed to create renderer");

	if (SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) != 0)
		throw exception("Failed to set draw colour");

	texture = SDL_CreateTexture(renderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, bufferWidth, bufferHeight);
	if (texture == nullptr)
		throw exception("Failed to create texture");

	bufferSize = BYTES_PER_PIXEL * bufferWidth * bufferHeight;
	frameBuffer = new uint8_t[bufferSize];
}

GemWindow::~GemWindow()
{
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}

float GemWindow::GetDpiAwareScalingFactor()
{
	if (_bFirstInstance)
	{
		if (!::SetProcessDPIAware())
		{
			throw exception("SetProcessDPIAware() returned false");
		}

		_bFirstInstance = false;
	}

	unsigned int dpi = ::GetDpiForSystem();
	float scale = float(dpi) / 96;

	return scale;
}

void GemWindow::DrawFrame(const ColourBuffer& new_frame)
{
	void* ptr;
	int pitch;

	if (SDL_LockTexture(texture, nullptr, &ptr, &pitch) == 0)
	{
		uint8_t* pixels = (uint8_t*)ptr;
		int index;

		for (int i = 0; i < new_frame.Count(); i++)
		{
			index = i * BYTES_PER_PIXEL;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			pixels[index + 0] = new_frame[i].Alpha;
			pixels[index + 1] = new_frame[i].Red;
			pixels[index + 2] = new_frame[i].Green;
			pixels[index + 3] = new_frame[i].Blue;
#else
			pixels[index + 0] = new_frame[i].Blue;
			pixels[index + 1] = new_frame[i].Green;
			pixels[index + 2] = new_frame[i].Red;
			pixels[index + 3] = new_frame[i].Alpha;
#endif
		}

		SDL_UnlockTexture(texture);
		SDL_RenderCopy(renderer, texture, nullptr, nullptr);
	}
}

void GemWindow::DrawRect(int w, int h, int x, int y, const GemColour& colour)
{
	static thread_local SDL_Rect rect;

	rect.x = x * scaleFactor;
	rect.y = y * scaleFactor;
	rect.w = w * scaleFactor;
	rect.h = h * scaleFactor;

	SDL_SetRenderDrawColor(renderer, colour.Red, colour.Green, colour.Blue, colour.Alpha);
	SDL_RenderFillRect(renderer, &rect);
}

void GemWindow::DrawString(const char* cstr, const GemColour& colour, int size, int x, int y)
{
	LoadFonts();
	if (mainFont == nullptr)
		return;

	SDL_Color sdl_colour;
	sdl_colour.r = colour.Red;
	sdl_colour.g = colour.Green;
	sdl_colour.b = colour.Blue;
	DrawString(cstr, sdl_colour, mainFont, size, x, y);
}

void GemWindow::DrawString(const char* cstr, const SDL_Color& sdl_colour, TTF_Font* font, int size, int x, int y)
{
	assert(x < bufferWidth && y < bufferHeight);

	// TODO: implement support for more colours in the cache
	static SDL_Color black = {0};
	glyphCache.Init(font, renderer, black); 
	glyphCache.DrawString(cstr, x * scaleFactor, y * scaleFactor);
}

void GemWindow::DrawDebugText(string str)
{
	LoadFonts();
	if (mainFont == nullptr)
		return;

	SDL_Color yellow = {255,195,0};
	DrawString(str.c_str(), yellow, mainFont, 20, 5, 5);
}

void GemWindow::Fill(const GemColour& colour)
{
	SDL_SetRenderDrawColor(renderer, colour.Red, colour.Green, colour.Blue, colour.Alpha);
	SDL_RenderClear(renderer);
}

void GemWindow::ZeroBuffer()
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
}

void GemWindow::Present()
{
	MonitorFrameRate();

	SDL_RenderPresent(renderer);
	frameCtr++;
}

void GemWindow::MonitorFrameRate()
{
	using namespace std::chrono;

	// Basically every FRAME_DELAY_ADJUSTMENT_MOD frames we calculate the average frame time
	// and increase or decrease fpsDelay based on that average. The important part is that
	// the timing and adjustment include overhead from calling sleep_for

	if (fpsDelay != 0)
		this_thread::sleep_for(milliseconds(fpsDelay));

	if (--adjustmentCtr == 0)
	{
		auto now = high_resolution_clock::now();

		if (lastTime.time_since_epoch().count() != 0)
		{
			duration<float, milli> time_taken = now - lastTime;
			avgFrameTime = time_taken.count() / FRAME_DELAY_ADJUSTMENT_MOD;

			if (!vsync)
			{
				float diff = simFrameTime - avgFrameTime;
				if (diff >= 1.0f)
					fpsDelay++;
				else if (diff <= -1.0f)
					fpsDelay = fpsDelay > 0 ? fpsDelay - 1 : 0;
			}
		}

		lastTime = now;
		adjustmentCtr = FRAME_DELAY_ADJUSTMENT_MOD;
	}

	if (showFPS && avgFrameTime != 0)
	{
		stringstream ss;
		ss << "FPS: " << fixed << setprecision(2) << (1000 / avgFrameTime) << " Hz";
		DrawDebugText(ss.str());
	}
}

void GemWindow::SetFrameRateLimit(unsigned int limit) 
{
	if (limit == 0)
	{
		fpsDelay = 0;
		simFrameTime = 0;
	}
	else
	{
		simFrameTime = 1000.0 / float(limit);
		// Reduce the actual delay by 40% to account for overhead from sleeping
		fpsDelay = int(simFrameTime * 0.6);
	}

	avgFrameTime = 0;
	adjustmentCtr = FRAME_DELAY_ADJUSTMENT_MOD;
	frLimit = limit;
}

void GemWindow::SetTitle(const char* title)
{
	SDL_SetWindowTitle(window, title);
}

bool GemWindow::IsOpen()
{
	return (SDL_GetWindowFlags(window) & SDL_WindowFlags::SDL_WINDOW_SHOWN) != 0;
}

void GemWindow::Open() 
{
	SDL_ShowWindow(window);
}

void GemWindow::Close()
{
	SDL_HideWindow(window);
}
