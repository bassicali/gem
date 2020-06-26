
#pragma once

#include <vector>
#include <memory>

#include "Core/GemConstants.h"
#include "DArray.h"

class SoundEmitter
{
	friend class Channel1Registers;
	friend class Channel2Registers;
	friend class Channel3Registers;
	friend class Channel4Registers;
	friend class APU;

public:
	SoundEmitter::SoundEmitter(int channel_number);
	void Tick();
	void Start() { emit = true; }
	void Stop() { emit = false; }

	virtual void TickTClock() = 0;
	virtual void Tick256Hz();
	virtual void Tick128Hz() {}
	virtual void Tick64Hz() {}
	virtual ~SoundEmitter() {}

	const int ChannelNo() const { return channelNum; }
	const bool IsEmitting() const { return gatedAmplitude > 0; }
	const uint8_t Sample() const { return gatedAmplitude; }
	void Mute() { emit = false; }

protected:
	bool emit;
	uint8_t amplitude;
	uint8_t gatedAmplitude;

	int channelNum;
	bool dacOn;

	int sequencerTAcc;
	int sequencerStep;

	// All 4 channels have this feature
	uint8_t soundLenCtr;
	bool stopAfterLen;
};

// Used to test the mixer
class SinusoidEmitter : public SoundEmitter
{
public:
	SinusoidEmitter();
	virtual void TickTClock() override;
private:
	double x;
};

class SquareWaveEmitter : public SoundEmitter
{
public:
	friend class Channel1Registers;
	friend class Channel2Registers;

	SquareWaveEmitter(int channel_number);
	virtual void TickTClock() override;
	virtual void Tick128Hz() override;
	virtual void Tick64Hz() override;
	void SetWavePeriodData(uint16_t value);
	void Reset(uint16_t freq_data, uint8_t freq_sweep_time, bool freq_decreasing, uint8_t freq_divider, uint8_t duty_cycle, uint8_t initial_envelope_volume, bool volume_increasing, uint8_t envelope_steps, bool dac_on, uint8_t duration, bool stop_after_counter);

private:
	static const uint8_t DutyCycleLookUp[4][8];

	uint16_t periodData;
	int tCyclesPerWave;
	uint8_t freqDivider;

	// Freq state
	uint16_t waveTCtr;
	uint8_t dutyCycleCode;
	uint8_t sweepTicksPerShift;
	uint8_t sweepCtr; // Counts down until the frequency shifts
	bool freqDecreasing;
	int waveFormStep;

	// Envelope state
	bool volumeIncreasing;
	uint8_t envelopeTicksPerShift;
	uint8_t envelopeCtr;
};

class ProgrammableWaveEmitter : public SoundEmitter
{
	friend class Channel3Registers;
	friend class APU;

public:
	ProgrammableWaveEmitter(int channel_number, uint8_t wave_ram[]);
	virtual void TickTClock() override;
	void SetWavePeriodData(uint16_t value);
	void Reset(uint16_t freq_data, uint8_t duration, bool stop_after_counter, uint8_t amp_divider, bool dac_on);

private:
	uint8_t* waveRAM;
	int sampleIdx;
	uint8_t amplitudeDivider;

	uint16_t periodData;
	uint16_t waveTCtr;
	int tCyclesPerWave;
};

class NoiseEmitter : public SoundEmitter
{
	friend class Channel4Registers;

public:
	NoiseEmitter(int channel_number);
	virtual void TickTClock() override;
	virtual void Tick64Hz() override;
	void SetShiftPeriodData(uint8_t base_period_code, uint8_t multiplier);
	void Reset(uint8_t base_period_code, uint8_t multiplier, bool short_mode, uint8_t initial_envelope_volume, bool volume_increasing, uint8_t envelope_steps, bool dac_on, uint8_t duration, bool stop_after_counter);

private:
	uint8_t amplitude;

	uint16_t shiftRegister;
	int shiftCtr;
	int tCyclesPerShift;
	bool shortMode;

	// Envelope state
	bool volumeIncreasing;
	uint8_t envelopeTicksPerShift;
	uint8_t envelopeCtr;
};
