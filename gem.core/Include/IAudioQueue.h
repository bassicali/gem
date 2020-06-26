
#pragma once

#include "DArray.h"

class IAudioQueue
{
	public:
		virtual int GetQueuedSampleCount() = 0;
		virtual void PushAudioData(const DArray<float>& buffer) = 0;
};