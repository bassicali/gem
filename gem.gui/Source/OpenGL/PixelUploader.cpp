
#include <exception>

#include <glad/glad.h>

#include "Colour.h"
#include "Core/GPU.h"

#include "OpenGL/PixelUploader.h"
#include "OpenGL/Common.h"

PixelUploader::PixelUploader()
	: texId(0)
	, buffersCreated(false)
	, index(0)
	, clientBuffer(nullptr)
{
	pboIds[0] = 0;
	pboIds[1] = 0;
}

bool PixelUploader::Init(const ColourBuffer* buffer)
{
	clientBuffer = const_cast<ColourBuffer*>(buffer);
	return true;
}

void PixelUploader::Upload()
{
	if (!clientBuffer)
		return;

	ColourBuffer& gem_buffer = *clientBuffer;

	GLvoid* buffer = (GLvoid*)gem_buffer.Ptr();
	int count = gem_buffer.Count() * 4;

	if (!buffersCreated)
	{
		_GL_WRAP2(glGenTextures, 1, &texId);
		_GL_WRAP2(glBindTexture, GL_TEXTURE_2D, texId);
		_GL_WRAP3(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		_GL_WRAP3(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		_GL_WRAP3(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		_GL_WRAP3(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		_GL_WRAP9(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA8, gem_buffer.Width, gem_buffer.Height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, (GLvoid*)buffer);
		_GL_WRAP2(glBindTexture, GL_TEXTURE_2D, 0);


		_GL_WRAP2(glGenBuffers, 1, &pboIds[0]);
		_GL_WRAP2(glBindBuffer, GL_PIXEL_UNPACK_BUFFER, pboIds[0]);
		_GL_WRAP4(glBufferData, GL_PIXEL_UNPACK_BUFFER, count, 0, GL_STREAM_DRAW);

		_GL_WRAP2(glGenBuffers, 1, &pboIds[1]);
		_GL_WRAP2(glBindBuffer, GL_PIXEL_UNPACK_BUFFER, pboIds[1]);
		_GL_WRAP4(glBufferData, GL_PIXEL_UNPACK_BUFFER, count, 0, GL_STREAM_DRAW);

		buffersCreated = true;
	}

	index = (index + 1) % 2;
	int nextIndex = (index + 1) % 2;

	_GL_WRAP2(glBindTexture, GL_TEXTURE_2D, texId);
	_GL_WRAP2(glBindBuffer, GL_PIXEL_UNPACK_BUFFER, pboIds[index]);

	_GL_WRAP9(glTexSubImage2D, GL_TEXTURE_2D, 0, 0, 0, gem_buffer.Width, gem_buffer.Height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);

	// bind PBO to update texture source
	_GL_WRAP2(glBindBuffer, GL_PIXEL_UNPACK_BUFFER, pboIds[nextIndex]);

	// NOTE: glMapBuffer can stall if GPU is busy, glBufferData mitigates this
	_GL_WRAP4(glBufferData, GL_PIXEL_UNPACK_BUFFER, count, 0, GL_STREAM_DRAW);

	// map the buffer object into client's memory
	GLubyte* ptr = (GLubyte*)_GL_WRAP2(glMapBuffer, GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	if (ptr)
	{
		for (int i = 0; i < gem_buffer.Width * gem_buffer.Height * 4; i += 4)
		{
			// Little endian ordering
			ptr[i + 0] = 255;
			ptr[i + 1] = gem_buffer[i / 4].Blue;
			ptr[i + 2] = gem_buffer[i / 4].Green;
			ptr[i + 3] = gem_buffer[i / 4].Red;
		}

		_GL_WRAP1(glUnmapBuffer, GL_PIXEL_UNPACK_BUFFER);
	}

	_GL_WRAP2(glBindBuffer, GL_PIXEL_UNPACK_BUFFER, 0);
}