
#pragma once

#include <memory>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "DArray.h"
#include "IAudioQueue.h"

class APU;

class GemSoundStream : public IAudioQueue
{
public:
	GemSoundStream();
	bool Init(std::shared_ptr<APU> ptr);
	void Play();
	void Pause();
	virtual int GetQueuedSampleCount() override;
	virtual void PushAudioData(const DArray<float>& buffer) override;
	void Shutdown();
	const bool IsInitialized() const { return initialized; }
	const bool IsPlaying() const { return playing; }
private:
	SDL_AudioDeviceID device;
	std::shared_ptr<APU> apu;
	bool initialized;
	bool playing;
};
