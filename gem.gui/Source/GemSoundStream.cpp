
#include <cassert>

#include "Core/APU.h"
#include "Core/GemConstants.h"
#include "Logging.h"

#include "GemSoundStream.h"

using namespace std;

GemSoundStream::GemSoundStream()
	: playing(false)
	, initialized(false)
	, device(0)
{
}

bool GemSoundStream::Init(shared_ptr<APU> ptr)
{
	if (initialized)
		return false;	

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG_ERROR("Failed to initialize SDL audio: %s", SDL_GetError()); 
		return false;
	}

	SDL_AudioSpec requested, obtained;

	memset(&requested, 0, sizeof(SDL_AudioSpec));
	requested.freq = 44100;
	requested.format = AUDIO_F32SYS;
	requested.channels = 2;
	requested.samples = APU::AudioQueueSampleCount;
	requested.callback = nullptr;
	requested.userdata = nullptr;

	device = SDL_OpenAudioDevice(nullptr, 0, &requested, &obtained, 0);
	if (device == 0)
	{
		LOG_ERROR("Failed to open audio device: %s", SDL_GetError());
		return false;
	}

	if (obtained.format != requested.format)
	{
		LOG_ERROR("Audio format not supported");
		return false;
	}

	apu = ptr;
	apu->SetAudioQueue(this);
	playing = false;

	initialized = true;
	return true;
}

void GemSoundStream::Play()
{
	if (!initialized)
		return;

	SDL_PauseAudioDevice(device, 0);
	playing = true;
}

void GemSoundStream::Pause()
{
	if (!initialized)
		return;

	SDL_PauseAudioDevice(device, 1);
	playing = false;
}

int GemSoundStream::GetQueuedSampleCount()
{
	int size = SDL_GetQueuedAudioSize(device);
	return size / sizeof(float);
}

void GemSoundStream::ClearQueue()
{
	SDL_ClearQueuedAudio(device);
}

void GemSoundStream::PushAudioData(const DArray<float>& buffer)
{
	if (SDL_QueueAudio(device,
		(void*)buffer.Ptr(),
		buffer.Count() * sizeof(float)) != 0)
	{
		LOG_ERROR("SDL_QueueAudio failed: %s", SDL_GetError());
	}
}

void GemSoundStream::Shutdown()
{
	if (SDL_WasInit(SDL_INIT_AUDIO))
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}