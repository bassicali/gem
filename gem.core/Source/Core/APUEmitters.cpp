
#include <cassert>
#include <climits>
#include <cmath>

#include "Core/GemConstants.h"
#include "Core/APUEmitters.h"

using namespace std;
using namespace GemConstants;

/////////////////////////////
///     SoundEmitter      ///
/////////////////////////////
SoundEmitter::SoundEmitter(int channel_number)
	: emit(false),
		amplitude(0),
		gatedAmplitude(0),
		sequencerTAcc(0),
		sequencerStep(0),
		channelNum(channel_number)
{
}

void SoundEmitter::Tick()
{
	TickTClock();

	if (++sequencerTAcc >= TCyclesPerAPUCycle)
	{
		sequencerStep = (sequencerStep + 1) % 8;
		sequencerTAcc -= TCyclesPerAPUCycle;

		if (stopAfterLen && soundLenCtr > 0 && sequencerStep % 2 == 0)
			Tick256Hz();

		if (sequencerStep == 1 || sequencerStep == 5)
			Tick128Hz();

		if (sequencerStep == 6)
			Tick64Hz();
	}
}

void SoundEmitter::Tick256Hz()
{
	if (--soundLenCtr == 0)
		emit = false;
}

/////////////////////////////
///    Sinusoid Emitter   ///
/////////////////////////////
SinusoidEmitter::SinusoidEmitter()
	: SoundEmitter(0), x(0.0f)
{
}

void SinusoidEmitter::TickTClock()
{
	x += 1;
	float y = 8 * cos(2 * 3.14159265358979 * (x / 44100) * 100) + 8.01;
	gatedAmplitude = amplitude = uint16_t(y);
}

/////////////////////////////
///  Square Wave Emitter  ///
/////////////////////////////

const uint8_t SquareWaveEmitter::DutyCycleLookUp[4][8] =
{
	{ 1, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 1, 0, 0, 0, 0, 0, 0 },
	{ 1, 1, 1, 1, 0, 0, 0, 0 },
	{ 1, 1, 1, 1, 1, 1, 0, 0 }
};

SquareWaveEmitter::SquareWaveEmitter(int channel_number)
	: SoundEmitter(channel_number)
{
}

void SquareWaveEmitter::TickTClock()
{
	if (--waveTCtr == 0)
	{
		waveFormStep = (waveFormStep + 1) % 8;
		waveTCtr = tCyclesPerWave;
	}

	gatedAmplitude = 0;
	if (emit && dacOn && bool(DutyCycleLookUp[dutyCycleCode][waveFormStep]))
	{
		gatedAmplitude = amplitude;
	}
}

void SquareWaveEmitter::Tick128Hz()
{
	if (--sweepCtr == 0)
	{
		sweepCtr = sweepTicksPerShift > 0 ? sweepTicksPerShift : 8;

		if (freqDivider > 0)
		{
			uint16_t delta = periodData >> freqDivider;
			int new_period = freqDecreasing
				? periodData - delta
				: periodData + delta;

			if (new_period > 0 && new_period <= 2047)
			{
				periodData = new_period;
				tCyclesPerWave = 4 * (2048 - periodData);
			}
			else
			{
				emit = false;
			}
		}
	}
}

void SquareWaveEmitter::Tick64Hz()
{
	if (--envelopeCtr == 0)
	{
		if (envelopeTicksPerShift)
		{
			if (volumeIncreasing && amplitude < 0xF)
			{
				amplitude++;
			}
			else if (!volumeIncreasing && amplitude > 0)
			{
				amplitude--;
			}
		}

		envelopeCtr = envelopeTicksPerShift > 0 ? envelopeTicksPerShift : 8;
	}
}

void SquareWaveEmitter::SetWavePeriodData(uint16_t value)
{
	periodData = value;
	tCyclesPerWave = 4 * (2048 - value);
}

void SquareWaveEmitter::Reset(uint16_t period_data, uint8_t freq_sweep_time, bool freq_decreasing, uint8_t freq_divider, uint8_t duty_cycle_code, uint8_t initial_envelope_volume, bool volume_increasing, uint8_t envelope_steps, bool dac_on, uint8_t duration, bool stop_after_counter)
{
	SetWavePeriodData(period_data);
	waveTCtr = tCyclesPerWave;

	soundLenCtr = duration;
	stopAfterLen = stop_after_counter;
	dutyCycleCode = duty_cycle_code;
	waveFormStep = 0;

	freqDecreasing = freq_decreasing;
	freqDivider = freq_divider;

	sweepTicksPerShift = freq_sweep_time;
	sweepCtr = sweepTicksPerShift > 0 ? sweepTicksPerShift : 8;

	amplitude = initial_envelope_volume;
	volumeIncreasing = volume_increasing;
	envelopeTicksPerShift = envelope_steps;
	envelopeCtr = envelopeTicksPerShift;

	dacOn = dac_on;
	emit = true;
}

/////////////////////////////
///    Wave RAM Player    ///
/////////////////////////////

ProgrammableWaveEmitter::ProgrammableWaveEmitter(int channel_number, uint8_t wave_ram[])
	: SoundEmitter(channel_number),
		sampleIdx(0)
{
	waveRAM = wave_ram;
}

void ProgrammableWaveEmitter::TickTClock()
{
	if (--waveTCtr == 0)
	{
		sampleIdx = (sampleIdx + 1) % WaveRAMSize;
		waveTCtr = tCyclesPerWave;

		gatedAmplitude = 0;
		if (amplitudeDivider > 0 && emit && dacOn)
		{
			amplitude = waveRAM[sampleIdx / 2];
			amplitude = sampleIdx % 2 == 0
						? amplitude >> 4
						: amplitude & 0xF;
			amplitude = amplitude >> (amplitudeDivider - 1);
			gatedAmplitude = amplitude;
		}
	}
}

void ProgrammableWaveEmitter::SetWavePeriodData(uint16_t value)
{
	periodData = value;
	tCyclesPerWave = 2 * (2048 - value);
}

void ProgrammableWaveEmitter::Reset(uint16_t period_data, uint8_t duration, bool stop_after_counter, uint8_t amp_divider, bool dac_on)
{
	SetWavePeriodData(period_data);
	waveTCtr = tCyclesPerWave;
	soundLenCtr = duration;
	stopAfterLen = stop_after_counter;
	amplitudeDivider = amp_divider;

	sampleIdx = 0;
	emit = true;
}

/////////////////////////////
///      Noise Emitter    ///
/////////////////////////////

NoiseEmitter::NoiseEmitter(int channel_number)
	: SoundEmitter(channel_number)
{
	soundLenCtr = 0;
}

void NoiseEmitter::TickTClock()
{
	if (--shiftCtr == 0)
	{
		shiftCtr = tCyclesPerShift;

		uint16_t xored = (shiftRegister & 0x1) ^ ((shiftRegister & 0x2) >> 1);
		shiftRegister >>= 1;

		shiftRegister = (xored << 14) | (shiftRegister & 0x3FFF);
		if (shortMode)
		{
			shiftRegister = (xored << 6) | (shiftRegister & 0x7FBF);
		}

		gatedAmplitude = 0;
		if (emit && dacOn && (shiftRegister & 0x1) == 0)
		{
			gatedAmplitude = amplitude;
		}
	}
}

void NoiseEmitter::SetShiftPeriodData(uint8_t base_period_code, uint8_t multiplier)
{
	uint8_t base_period = base_period_code == 0
							? 8 : 16 * base_period_code;

	tCyclesPerShift = base_period << multiplier;
}

void NoiseEmitter::Tick64Hz()
{
	if (--envelopeCtr == 0)
	{
		if (envelopeTicksPerShift)
		{
			if (volumeIncreasing && amplitude < 0xF)
			{
				amplitude++;
			}
			else if (!volumeIncreasing && amplitude > 0)
			{
				amplitude--;
			}
		}

		envelopeCtr = envelopeTicksPerShift > 0 ? envelopeTicksPerShift : 8;
	}
}

void NoiseEmitter::Reset(uint8_t base_period_code, uint8_t multiplier, bool short_mode, uint8_t initial_envelope_volume, bool volume_increasing, uint8_t envelope_steps, bool dac_on, uint8_t duration, bool stop_after_counter)
{
	SetShiftPeriodData(base_period_code, multiplier);

	shiftCtr = tCyclesPerShift;
	shortMode = short_mode;

	soundLenCtr = duration;
	stopAfterLen = stop_after_counter;

	amplitude = initial_envelope_volume;
	volumeIncreasing = volume_increasing;
	envelopeTicksPerShift = envelope_steps;
	envelopeCtr = envelopeTicksPerShift;
	dacOn = dac_on;

	shiftRegister = 0x7FFF;
	emit = true;
}