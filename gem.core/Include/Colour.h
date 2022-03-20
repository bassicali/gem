
#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

#include "DArray.h"

#define COLOUR_PALETTE_PURPLE	0
#define COLOUR_PALETTE_GREEN	1
#define USE_PALETTE				1

#define DECLARE_COLOUR_STANDALONE(Name,R,G,B) static const GemColour& Name() { static GemColour* c = new GemColour((R),(G),(B)); return *c; }
#define DECLARE_COLOUR(Name,R,G,B) static const GemColour& Name() { if (!_##Name) { _##Name = new GemColour((R),(G),(B)); } return *_##Name; }
#define DECLARE_PIXEL(Name,R,G,B,A) static const Pixel& Name() { static Pixel* px = new Pixel(R,G,B,A); return *px; }

enum class CorrectionMode
{
	Washout,
	Scale
};

struct GemColour
{
	uint8_t Red;
	uint8_t Green;
	uint8_t Blue;
	uint8_t Alpha;

	// We need to store the colour number so we know which BG tiles were coloured from 0
	// so that sprites can be appear above these pixels.
	// This field is assigned when MonochromePallete::GetColour() or ColourPallete::GetColour() is called.
	uint8_t ColourNumber;

	// We also need to store this so that sprites don't overwrite BG pixels that have this bit set
	bool Priority;

	GemColour();
	GemColour(const GemColour& Other);
	GemColour(uint8_t red, uint8_t green, uint8_t blue);
	GemColour& operator=(const GemColour& Other);
	void Correct();
	void Correct(CorrectionMode mode, float brightness);
	bool ReplaceWithSpritePixel(const GemColour& replace_with, int colour_number, bool behind_bg, bool force);

	DECLARE_COLOUR_STANDALONE(Black, 0, 0, 0)
	DECLARE_COLOUR_STANDALONE(Purple, 112, 48, 160)
	DECLARE_COLOUR_STANDALONE(White, 255, 255, 255)
};

class GemPalette
{
public:
#if USE_PALETTE == COLOUR_PALETTE_GREEN
	DECLARE_COLOUR(Black, 0, 0, 0)
	DECLARE_COLOUR(DarkGrey, 48, 108, 80)
	DECLARE_COLOUR(LightGrey, 136, 192, 112)
	DECLARE_COLOUR(White, 224, 248, 208)
#else
	DECLARE_COLOUR(Black, 208, 57, 127)
	DECLARE_COLOUR(DarkGrey, 249, 99, 152)
	DECLARE_COLOUR(LightGrey, 252, 167, 184)
	DECLARE_COLOUR(White, 250, 255, 206)
#endif

	static void ReAssign(int colour_number, const GemColour& colour);

private:
	static GemColour* _Black;
	static GemColour* _DarkGrey;
	static GemColour* _LightGrey;
	static GemColour* _White;
};

#undef DECLARE_COLOUR

struct ColourBuffer : public DArray<GemColour>
{
	ColourBuffer();
	ColourBuffer(int w, int h);
	void CorrectPixel(int x, int y, CorrectionMode mode = CorrectionMode::Washout);
	void SetPixel(int x, int y, const GemColour& Colour);
	const GemColour* Copy(GemColour* dest = nullptr) const;
	void Zero();

	int Width;
	int Height;
};