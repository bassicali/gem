
#pragma once

#include "Colour.h"

class IDrawTarget
{
	public:
		virtual void DrawFrame(const ColourBuffer& new_frame) = 0;
		virtual void DrawString(const char* str, const GemColour& colour, int size, int x, int y) = 0;
		virtual void DrawRect(int w, int h, int x, int y, const GemColour& colour) = 0;
		virtual void Fill(const GemColour& colour) = 0;
		virtual ~IDrawTarget()
		{
		}
};