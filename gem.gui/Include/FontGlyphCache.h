
#pragma once

#include <string>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#define NUM_GLYPHS 95

struct GlyphTexture
{
	SDL_Texture* Texture;
	int Width;
	int Height;

	GlyphTexture()
	{
	}

	GlyphTexture(SDL_Texture* tex, int w, int h)
		: Texture(tex), Width(w), Height(h)
	{
	}
};

class FontGlyphCache
{
public:
	FontGlyphCache();
	void Init(TTF_Font* font, SDL_Renderer* renderer, const SDL_Color& colour);
	~FontGlyphCache();

	void DrawString(const char* text, int x, int y);

private:
	bool initialized;
	GlyphTexture glyphs[NUM_GLYPHS];
	SDL_Renderer* renderer;
};