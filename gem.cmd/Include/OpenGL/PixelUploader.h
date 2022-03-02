
#pragma once

#include "IDrawTarget.h"

struct ColourBuffer;

class PixelUploader
{
public:
	PixelUploader();
	bool Init(const ColourBuffer* buffer);
	void Upload();

	ColourBuffer* BufferPtr() const { return clientBuffer; }
	bool IsInitialized() const { return clientBuffer != nullptr; }

	int TextureId() const { return texId; }

private:
	unsigned int texId;
	ColourBuffer* clientBuffer;
	unsigned int pboIds[2];
	bool buffersCreated;
	int index;
};
