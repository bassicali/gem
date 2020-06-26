#pragma once

#include <iostream>
#include <filesystem>
#include <string>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#include <windows.h>

#include "IDrawTarget.h"
#include "FontGlyphCache.h"
#include "IAudioQueue.h"
#include "Logging.h"
#include "Core/GPURegisters.h"
#include "Core/Joypad.h"

#define BYTES_PER_PIXEL 4
#define FRAME_DELAY_ADJUSTMENT_MOD 4

#define TO_SDL_COLOUR(c) sf::Color((c).Red, (c).Green, (c).Blue, 255)
#define FONT_NAME "CascadiaMono.ttf"

using namespace std;
namespace fs = std::filesystem;

struct _StaticFontLoader
{
	_StaticFontLoader(TTF_Font** main, int main_sz)
	{
		char buff[_MAX_PATH];
		fs::path font_path;
		if (GetModuleFileNameA(nullptr, buff, _MAX_PATH))
		{
			font_path = fs::absolute(fs::path(buff)).parent_path() / FONT_NAME;
		}
		else
		{
			font_path = FONT_NAME;
		}

		string path = font_path.string();

		*main = TTF_OpenFont(path.c_str(), main_sz);
		if (*main == nullptr)
		{
			LOG_ERROR("TTF_OpenFont failed for %s: %s", FONT_NAME, TTF_GetError());
		}
	}
};

class GemWindow : public IDrawTarget
{
	public:
		GemWindow(const char* title, int buff_width, int buff_height, IAudioQueue* queue, bool vsync, float scale = 1);
		~GemWindow();
		virtual void DrawFrame(const ColourBuffer& new_frame) override;
		virtual void DrawRect(int w, int h, int x, int y, const GemColour& colour) override;
		virtual void DrawString(const char* cstr, const GemColour& color, int size, int x, int y) override;
		virtual void Fill(const GemColour& colour) override;
		void DrawString(const char* cstr, const SDL_Color& color, TTF_Font* font, int size, int x, int y);
		void Present();
		int GetFrameRateLimit() const { return frLimit; }
		void SetFrameRateLimit(unsigned int limit);
		void ZeroBuffer();
		void SetTitle(const char* title);

		const int Id() const { return windowId; }
		void Open();
		void Close();
		bool IsOpen();

		void ShowFPSCounter(bool enable) { showFPS = enable; }

		static const int DefaultFrameRate = 60;
		static const int FontSize = 14;

	private:
		static bool _bFirstInstance;
		
		float GetDpiAwareScalingFactor();
		void MonitorFrameRate();

		static void LoadFonts()
		{
			static _StaticFontLoader* loader = new _StaticFontLoader(&mainFont, 18);
		}

		static TTF_Font* mainFont;

		FontGlyphCache glyphCache;
		float scaleFactor;
		int windowWidth;
		int windowHeight;
		int bufferWidth;
		int bufferHeight;
		size_t bufferSize;

		bool showFPS;
		unsigned long frameCtr;

		void DrawDebugText(std::string str);

		SDL_Window* window;
		int windowId;

		SDL_Rect windowRect;
		SDL_Renderer* renderer;
		SDL_Texture* texture;
		
		uint8_t* frameBuffer;
		bool vsync;
		int frLimit;
		float simFrameTime;
		float avgFrameTime;
		int fpsDelay;
		int adjustmentCtr;
		std::chrono::steady_clock::time_point lastTime;
};
