
#include "Colour.h"

//////////////////////////
///      RGB Colour    ///
//////////////////////////

GemColour::GemColour() :
	Red(0x1F), Green(0x1F), Blue(0x1F), Alpha(0xFF), ColourNumber(0), Priority(false)
{
}

GemColour::GemColour(uint8_t red, uint8_t green, uint8_t blue) : Red(red), Green(green), Blue(blue), Alpha(0xFF), ColourNumber(0), Priority(false)
{
}

GemColour::GemColour(const GemColour& Other)
	: Red(Other.Red), Green(Other.Green), Blue(Other.Blue), Alpha(Other.Alpha), ColourNumber(Other.ColourNumber), Priority(Other.Priority)
{
}

GemColour& GemColour::operator=(const GemColour& Other)
{
	Red = Other.Red;
	Green = Other.Green;
	Blue = Other.Blue;
	Alpha = Other.Alpha;
	ColourNumber = Other.ColourNumber;
	Priority = Other.Priority;
	return *this;
}

void GemColour::Correct()
{
	Correct(CorrectionMode::Washout, 1.0f);
}

void GemColour::Correct(CorrectionMode mode, float brightness)
{
#define MIN(x,y) (x < y ? x : y)

#pragma region Explanation
// Found this formula at https://byuu.net/video/color-emulation (a good read, I recommend checking it out)
// Unfortunately the original author isn't known by byuu's author either.
// But it's fairly straightforward what it's doing. The sum of products is basically distributing a little 
// bit of energy/intensity from each channel to the other two channels. The sum of the coefficients is 32 
// which is the highest intensity. The net effect of this distribution of energy is that the colours become
// "washed out" and less saturated as they would have on the LCD screens of the Gameboy. Note that if the
// coefficients were evenly distributed (i.e. the energy in each channel was evenly split amongst all 3) it'd
// produce a greyscale image, in other words, the greyness gets maxmized.
#pragma endregion

	if (mode == CorrectionMode::Washout)
	{
		int r = Red;
		int g = Green;
		int b = Blue;

		r = (26 * Red) + (4 * Green) + (2 * Blue);
		g = (24 * Green) + (8 * Blue);
		b = (6 * Red) + (4 * Green) + (22 * Blue);

		Red = uint8_t(MIN(960, r) / 4);
		Green = uint8_t(MIN(960, g) / 4);
		Blue = uint8_t(MIN(960, b) / 4);
	}
	else
	{
		Red *= 8;
		Green *= 8;
		Blue *= 8;
	}

	if (brightness > 1.0f)
	{
		Red = uint8_t(std::min(brightness * Red, 255.0f));
		Green = uint8_t(std::min(brightness * Green, 255.0f));
		Blue = uint8_t(std::min(brightness * Blue, 255.0f));
	}

#undef MIN
}

// This helper function accounts for some extra rules when it comes to rendering sprites
// in relation to the BG.
// 1. Sprite pixels with colour 0 are invisible.
// 2. In CGB mode BG tiles can have absolute priority over sprites
// 3. Sprites can be above or behind the BG. If they are behind the BG, they will still 
//    appear above BG pixels that were coloured with 0.
// 4. Colour 0 is always painted over by a sprite
bool GemColour::ReplaceWithSpritePixel(const GemColour& replace_with, int colour_number, bool behind_bg, bool force)
{
	if (colour_number == 0 || (Priority && ColourNumber != 0))
		return false;

	if (force || (behind_bg && ColourNumber == 0) || !behind_bg)
	{
		Red = replace_with.Red;
		Green = replace_with.Green;
		Blue = replace_with.Blue;
		ColourNumber = colour_number;
		return true;
	}

	return false;
}

/////////////////////////////////
///      Colour Palette       ///
/////////////////////////////////
GemColour* GemPalette::_Black = nullptr;
GemColour* GemPalette::_DarkGrey = nullptr;
GemColour* GemPalette::_LightGrey = nullptr;
GemColour* GemPalette::_White = nullptr;

void GemPalette::ReAssign(int colour_number, const GemColour& colour)
{
	switch (colour_number)
	{
	case 0:
		*_White = colour;
		_White->ColourNumber = colour_number;
		break;
	case 1:
		*_LightGrey = colour;
		_LightGrey->ColourNumber = colour_number;
		break;
	case 2:
		*_DarkGrey = colour;
		_DarkGrey->ColourNumber = colour_number;
		break;
	case 3:
		*_Black = colour;
		_Black->ColourNumber = colour_number;
		break;
	}
}

/////////////////////////////////
///       Colour Buffer       ///
/////////////////////////////////

ColourBuffer::ColourBuffer()
	: DArray<GemColour>()
	, Width(0)
	, Height(0)
{

}

ColourBuffer::ColourBuffer(int w, int h)
	: DArray<GemColour>(w * h, true)
	, Width(w)
	, Height(h)
{
}

void ColourBuffer::CorrectPixel(int x, int y)
{
	int index = (y * Width) + x;
	data[index].Correct();
}

void ColourBuffer::SetPixel(int x, int y, const GemColour& Colour)
{
	int index = (y * Width) + x;
	data[index] = Colour;
}

const GemColour* ColourBuffer::Copy(GemColour* dest) const
{
	if (dest == nullptr)
	{
		dest = new GemColour[count];
	}
	memcpy(dest, data, count * sizeof(GemColour));
	return dest;
}

void ColourBuffer::Zero()
{
	memset(data, 0, Size());
}