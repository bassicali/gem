
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <functional>

#include "DArray.h"
#include "IAudioQueue.h"
#include "Core/APURegisters.h"

#define NUM_EMITTERS 4

class APU
{
public:
	APU();
	void SetAudioQueue(IAudioQueue* queue);
	void Reset();
	void TickEmitters(int m_cycles);

	uint8_t ReadRegister(uint16_t addr);
	void WriteRegister(uint16_t addr, uint8_t value);

	uint8_t ReadWaveRAM(uint16_t addr);
	void WriteWaveRAM(uint16_t addr, uint8_t value);
	
	void SetChannelMask(int chan_index, uint8_t mask);
	void DecodeWaveRAM(uint8_t dest[32]);
	int BufferCount() const { return buffer.Count(); }
	void ClearBuffer() { buffer.Truncate(0); }
	APUControlRegisters& Controller() { return controller; }

	static const int AudioQueueSampleCount = 4096;

private:
	static const int AmplitudeScale = SHRT_MAX / (NUM_EMITTERS * 8); // Max volume level = 8; Num channels = 4
	
	uint8_t waveRAM[GemConstants::WaveRAMSizeBytes];

	// An additional mask that the front-end can use to mute channels
	uint8_t chanMask[NUM_EMITTERS];
	
	APUControlRegisters controller;
	Channel1Registers chan1;
	Channel2Registers chan2;
	Channel3Registers chan3;
	Channel4Registers chan4;

	int sampleTAcc;
	SquareWaveEmitter emitter1;
	SquareWaveEmitter emitter2;
	ProgrammableWaveEmitter emitter3;
	NoiseEmitter emitter4;
	DArray<float> buffer;
	IAudioQueue* audioQueue;
};