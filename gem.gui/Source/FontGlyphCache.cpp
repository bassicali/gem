
#include "FontGlyphCache.h"

using namespace std;

FontGlyphCache::FontGlyphCache()
	: renderer(nullptr),
		initialized(false)
{
}

FontGlyphCache::~FontGlyphCache()
{
	if (!initialized)
		return;

	for (int i = 0; i < NUM_GLYPHS; i++)
		SDL_DestroyTexture(glyphs[i].Texture);
}

void FontGlyphCache::Init(TTF_Font* font, SDL_Renderer* renderer, const SDL_Color& colour)
{
	if (initialized)
		return;

	char buff[2];

	for (int i = 0; i < NUM_GLYPHS; i++)
	{
		buff[0] = (char)(i + 32);
		buff[1] = '\0';

		if (SDL_Surface* surface = TTF_RenderText_Blended(font, buff, colour))
		{
			if (SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface))
			{
				glyphs[i] = GlyphTexture(tex, surface->w, surface->h);
			}
			else
			{
				throw exception("Unable to create glyph cache");
			}

			SDL_FreeSurface(surface);
		}
		else
		{
			throw exception("Unable to create glyph cache");
		}
	}

	this->renderer = renderer;
	initialized = true;
}

void FontGlyphCache::DrawString(const char* text, int x, int y)
{
	char c;
	int index;
	int offsetx = x;

	SDL_Rect rect;
	rect.y = y;

	while (c = *text++)
	{
		index = c - 32;
		if (index < 0 && index >= NUM_GLYPHS)
			index = '?' - 32;

		rect.x = offsetx;
		rect.w = glyphs[index].Width;
		rect.h = glyphs[index].Height;
		offsetx += rect.w;

		if (SDL_RenderCopy(renderer, glyphs[index].Texture, nullptr, &rect) != 0)
			break;
	}
}