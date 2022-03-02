
#include <algorithm>
#include <cassert>

#include "Core/APU.h"
#include "Core/Z80.h"
#include "Logging.h"

using namespace std;

APU::APU()
	: buffer(8192)
	, audioQueue(nullptr)
	, emitter1(1)
	, emitter2(2)
	, emitter3(3, waveRAM)
	, emitter4(4)
	, sampleTAcc(0)
	, mute(false)
{
	buffer.Allocate();

	memset(waveRAM, 0, sizeof(uint8_t) * GemConstants::WaveRAMSizeBytes);

	chan1.SetEmitter(&emitter1);
	chan2.SetEmitter(&emitter2);
	chan3.SetEmitter(&emitter3);
	chan4.SetEmitter(&emitter4);

	chanMask[0] = 1;
	chanMask[1] = 1;
	chanMask[2] = 1;
	chanMask[3] = 1;
}

void APU::Reset()
{
	ClearBuffer();
	// TODO: reset the register values (probably not noticeable since sound channels are constantly resetting
}

void APU::SetAudioQueue(IAudioQueue* queue)
{
	audioQueue = queue;
}

void MixSamples(float& dest, float src, float volume)
{
	// TODO: what to do when all 4 channels are at full level? Then the mixed sum could be at most 4
	src = src * volume;
	dest = min(dest + src, FLT_MAX);
}

void APU::TickEmitters(int t_cycles)
{
#define MIXLR(index,amplitude) \
	MixSamples(lmixed, float(amplitude) / 15, float(left_levels[index]) / 7); \
	MixSamples(rmixed, float(amplitude) / 15, float(right_levels[index]) / 7);

	static uint8_t left_levels[4];
	static uint8_t right_levels[4];

	if (!controller.IsSoundOn())
		return;

	static float lmixed, rmixed, level;
	uint8_t m = mute ? 0 : 1;

	while (t_cycles--)
	{
		emitter1.Tick();
		emitter2.Tick();
		emitter3.Tick();
		emitter4.Tick();

		if (sampleTAcc == GemConstants::TCyclesPerSample)
		{
			controller.GetLeftLevelMask(left_levels);
			controller.GetRightLevelMask(right_levels);

			uint8_t e1 = emitter1.Sample();
			uint8_t e2 = emitter2.Sample();
			uint8_t e3 = emitter3.Sample();
			uint8_t e4 = emitter4.Sample();

			snapshot.L_Emitter1 = e1 * left_levels[0];
			snapshot.L_Emitter2 = e2 * left_levels[1];
			snapshot.L_Emitter3 = e3 * left_levels[2];
			snapshot.L_Emitter4 = e4 * left_levels[3];

			snapshot.R_Emitter1 = e1 * left_levels[0];
			snapshot.R_Emitter2 = e2 * left_levels[1];
			snapshot.R_Emitter3 = e3 * left_levels[2];
			snapshot.R_Emitter4 = e4 * left_levels[3];

			left_levels[0] *= chanMask[0];
			left_levels[1] *= chanMask[1];
			left_levels[2] *= chanMask[2];
			left_levels[3] *= chanMask[3];

			right_levels[0] *= chanMask[0];
			right_levels[1] *= chanMask[1];
			right_levels[2] *= chanMask[2];
			right_levels[3] *= chanMask[3];

			lmixed = 0;
			rmixed = 0;

			MixSamples(lmixed, float(e1) / 15, float(left_levels[0]) / 7);
			MixSamples(rmixed, float(e1) / 15, float(right_levels[0]) / 7);

			MixSamples(lmixed, float(e2) / 15, float(left_levels[1]) / 7);
			MixSamples(rmixed, float(e2) / 15, float(right_levels[1]) / 7);

			MixSamples(lmixed, float(e3) / 15, float(left_levels[2]) / 7);
			MixSamples(rmixed, float(e3) / 15, float(right_levels[2]) / 7);

			MixSamples(lmixed, float(e4) / 15, float(left_levels[3]) / 7);
			MixSamples(rmixed, float(e4) / 15, float(right_levels[3]) / 7);

			buffer.PushBack(lmixed * m);
			buffer.PushBack(rmixed * m);
			sampleTAcc = 0;
		}
		else
		{
			sampleTAcc++;
		}
	}

	if (audioQueue && buffer.Count() >= AudioQueueSampleCount * 2)
	{
		// Allow audio queue to catch up slightly
		int queued_count = audioQueue->GetQueuedSampleCount();
		if (queued_count > AudioQueueSampleCount * 2)
		{
			int ms = int((float(queued_count) * 0.4) / 44.1f);
			this_thread::sleep_for(chrono::milliseconds(ms));
			LOG_DIAG("Audio catch up (queued: %d | sleep: %d)", queued_count, ms);
		}

		audioQueue->PushAudioData(buffer);
		buffer.Truncate(0);
	}

	controller.Chan1SetSoundOn(emitter1.IsEmitting());
	controller.Chan2SetSoundOn(emitter2.IsEmitting());
	controller.Chan3SetSoundOn(emitter3.IsEmitting());
	controller.Chan4SetSoundOn(emitter4.IsEmitting());

#undef MIXLR
}

void APU::SetChannelMask(int chan_index, uint8_t mask)
{
	if (chan_index >= 0 && chan_index < 4)
		chanMask[chan_index] = mask;
}

void APU::DecodeWaveRAM(uint8_t dest[32])
{
	for (int i = 0, j = 0; i < 16; i++, j += 2)
	{
		dest[j] = waveRAM[i] >> 4;
		dest[j + 1] = waveRAM[i] & 0xF;
	}
}

uint8_t APU::ReadWaveRAM(uint16_t addr)
{
	int index = addr & 0xF;
	return waveRAM[index];
}

void APU::WriteWaveRAM(uint16_t addr, uint8_t value)
{
	int index = addr & 0xF;
	waveRAM[index] = value;
}

uint8_t APU::ReadRegister(uint16_t addr)
{
	switch (addr)
	{
		// Channel 1
		case 0xFF10:
			return chan1.ReadSweepRegister();
		case 0xFF11:
			return chan1.ReadWaveDutyAndLengthRegister();
		case 0xFF12:
			return chan1.ReadVolumeEnvelopeRegister();
		case 0xFF13:
			LOG_VIOLATION("%X is a write-only register", addr);
			break;
		case 0xFF14:
			return chan1.ReadFrequencyRegister();

		// Channel 2
		case 0xFF16:
			return chan2.ReadWaveDutyAndLengthRegister();
		case 0xFF17:
			return chan2.ReadVolumeEnvelopeRegister();
		case 0xFF18:
			LOG_VIOLATION("%X is a write-only register", addr);
			break;
		case 0xFF19:
			return chan2.ReadFrequencyRegister();

		// Channel 3
		case 0xFF1A:
			return chan3.ReadOnOffRegister();
		case 0xFF1B:
			return chan3.ReadSoundLengthRegister();
		case 0xFF1C:
			return chan3.ReadSoundLevelRegister();
		case 0xFF1D:
			LOG_VIOLATION("%X is a write-only register", addr);
			break;
		case 0xFF1E:
			return chan3.ReadFrequencyRegister();

		// Channel 4
		case 0xFF20:
			return chan4.ReadSoundLengthRegister();
		case 0xFF21:
			return chan4.ReadVolumeEnvelopeRegister();
		case 0xFF22:
			return chan4.ReadPolynomialRegister();
		case 0xFF23:
			return chan4.ReadTriggerRegister();

		// Controller
		case 0xFF24:
			return controller.ReadVolumeRegister();
		case 0xFF25:
			return controller.ReadChannelSelectionRegister();
		case 0xFF26:
			return controller.ReadChannelEnableRegister();
		default:
			LOG_VIOLATION("Unknown APU register: %X", addr);
	}

	return 0;
}

void APU::WriteRegister(uint16_t addr, uint8_t value)
{
	switch (addr)
	{
		// Channel 1
		case 0xFF10:
			chan1.WriteSweepRegister(value);
			break;
		case 0xFF11:
			chan1.WriteWaveDutyAndLengthRegister(value);
			break;
		case 0xFF12:
			chan1.WriteVolumeEnvelopeRegister(value);
			break;
		case 0xFF13:
			chan1.WriteFrequencyRegisterLow(value);
			break;
		case 0xFF14:
			chan1.WriteFrequencyRegisterHigh(value);
			if (chan1.ShouldStart())
			{
				chan1.StartEmitter();
				controller.Chan1SetSoundOn(true); // TODO: clear this bit after sound length is finished
			}
			break;

		// Channel 2
		case 0xFF16:
			chan2.WriteWaveDutyAndLengthRegister(value);
			break;
		case 0xFF17:
			chan2.WriteVolumeEnvelopeRegister(value);
			break;
		case 0xFF18:
			chan2.WriteFrequencyRegisterLow(value);
			break;
		case 0xFF19:
			chan2.WriteFrequencyRegisterHigh(value);
			if (chan2.ShouldStart())
			{
				chan2.StartEmitter();
				controller.Chan2SetSoundOn(true);
			}
			break;

		// Channel 3
		case 0xFF1A:
			chan3.WriteOnOffRegister(value);
			break;
		case 0xFF1B:
			chan3.WriteSoundLengthRegister(value);
			break;
		case 0xFF1C:
			chan3.WriteSoundLevelRegister(value);
			break;
		case 0xFF1D:
			chan3.WriteFrequencyRegisterLow(value);
			break;
		case 0xFF1E:
			chan3.WriteFrequencyRegisterHigh(value);
			if (chan3.ShouldStart())
			{
				chan3.StartEmitter();
				controller.Chan3SetSoundOn(true);
			}
			break;

		// Channel 4
		case 0xFF20:
			chan4.WriteSoundLengthRegister(value);
			break;
		case 0xFF21:
			chan4.WriteVolumeEnvelopeRegister(value);
			break;
		case 0xFF22:
			chan4.WritePolynomialRegister(value);
			break;
		case 0xFF23:
			chan4.WriteTriggerRegister(value);
			if (chan4.ShouldStart())
			{
				chan4.StartEmitter();
				controller.Chan4SetSoundOn(true);
			}
			break;

			// Controller
		case 0xFF24:
			controller.WriteVolumeRegister(value);
			break;
		case 0xFF25:
			controller.WriteChannelSelectionRegister(value);
			break;
		case 0xFF26:
			controller.WriteChannelEnableRegister(value);
			break;

		default:
			LOG_VIOLATION("Unknown APU register: %X", addr);
	}
}
