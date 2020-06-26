
#pragma once

#include <vector>
#include <cstdint>

#include "Core/APUEmitters.h"

class Channel2Registers
{
public:
	Channel2Registers();
	void SetEmitter(SquareWaveEmitter* ptr) { emitter = ptr; }

	void WriteWaveDutyAndLengthRegister(const uint8_t value);
	uint8_t ReadWaveDutyAndLengthRegister() const;

	void WriteVolumeEnvelopeRegister(const uint8_t value);
	uint8_t ReadVolumeEnvelopeRegister() const;

	void WriteFrequencyRegisterHigh(const uint8_t value);
	void WriteFrequencyRegisterLow(const uint8_t value);
	uint8_t ReadFrequencyRegister() const;

	const bool ShouldStart() const { return startPlaying; }
	virtual void StartEmitter();

protected:
	SquareWaveEmitter* emitter;

	// FF11 or FF16
	uint8_t waveDutyCode;
	uint8_t soundLength;
	uint8_t waveDutyAndLengthRegisterByte;

	// FF12 or FF17
	uint8_t initialVolume;
	bool volumeIncrease;
	uint8_t envelopePeriod;
	uint8_t volumeEnvelopeRegisterByte;


	// FF13 + FF14 or FF18 + FF19
	uint16_t periodData;
	bool startPlaying;
	bool stopAfterLen;
	uint8_t freqRegisterByte;
};

class Channel1Registers : public Channel2Registers
{
public:
	Channel1Registers();
	void WriteSweepRegister(const uint8_t value);
	uint8_t ReadSweepRegister() const;
	virtual void StartEmitter() override;

protected:
	// FF10
	uint8_t sweepPeriod; // Bit 6-4
	bool decrease; // Bit 3
	uint8_t freqDivider; // Bit 2-0
	uint8_t sweepRegisterByte;
};

class Channel3Registers
{
public:
	Channel3Registers();
	void SetEmitter(ProgrammableWaveEmitter* ptr) { emitter = ptr; }

	void WriteOnOffRegister(const uint8_t value);
	uint8_t ReadOnOffRegister() const;

	void WriteSoundLengthRegister(const uint8_t value);
	uint8_t ReadSoundLengthRegister() const;

	void WriteSoundLevelRegister(const uint8_t value);
	uint8_t ReadSoundLevelRegister() const;

	void WriteFrequencyRegisterHigh(const uint8_t value);
	void WriteFrequencyRegisterLow(const uint8_t value);
	uint8_t ReadFrequencyRegister() const;

	const bool ShouldStart() const { return startPlaying; }
	void StartEmitter();

	const bool On() const { return channelOn; }

protected:
	ProgrammableWaveEmitter* emitter;

	// FF1A
	bool channelOn; // Controls the DAC
	
	// FF1B
	uint8_t soundLength;

	// FF1C
	uint8_t amplitudeDivider;
	uint8_t shiftRegisterByte;

	// FF1D + FF1E
	uint16_t periodData;
	bool startPlaying;
	bool stopAfterLen;
	uint8_t freqRegisterByte;
};

class Channel4Registers
{
public:
	Channel4Registers();
	void SetEmitter(NoiseEmitter* ptr) { emitter = ptr; }

	void WriteSoundLengthRegister(const uint8_t value);
	uint8_t ReadSoundLengthRegister() const;

	void WriteVolumeEnvelopeRegister(const uint8_t value);
	uint8_t ReadVolumeEnvelopeRegister() const;

	void WritePolynomialRegister(const uint8_t value);
	uint8_t ReadPolynomialRegister();

	void WriteTriggerRegister(const uint8_t value);
	uint8_t ReadTriggerRegister();

	const bool ShouldStart() const { return startPlaying; }
	void StartEmitter();

private:
	NoiseEmitter* emitter;

	// FF20
	uint8_t soundLength;

	// FF21
	uint8_t initialVolume;
	bool volumeIncrease;
	uint8_t envelopePeriod;
	uint8_t volumeEnvelopeRegisterByte;

	// FF22
	uint8_t shiftClockMultiplier;
	bool shortMode;
	uint8_t periodData;
	uint8_t polynomialRegisterByte;

	// FF23
	bool startPlaying;
	bool stopAfterLen;
	uint8_t triggerRegisterByte;
};

class APUControlRegisters
{
public:
	APUControlRegisters();

	void WriteVolumeRegister(const uint8_t value);
	uint8_t ReadVolumeRegister() const;

	void WriteChannelSelectionRegister(const uint8_t value);
	uint8_t ReadChannelSelectionRegister() const;

	void WriteChannelEnableRegister(const uint8_t value);
	uint8_t ReadChannelEnableRegister() const;

	const uint8_t LeftLevel() const { return SO2Level; }
	const uint8_t RightLevel() const { return SO1Level; }

	void GetLeftLevelMask(uint8_t* arr);
	void GetRightLevelMask(uint8_t* arr);

	const bool IsSoundOn() const { return MasterEnable; }
	void Chan1SetSoundOn(bool on) { Channel1Enable = on; }
	void Chan2SetSoundOn(bool on) { Channel2Enable = on; }
	void Chan3SetSoundOn(bool on) { Channel3Enable = on; }
	void Chan4SetSoundOn(bool on) { Channel4Enable = on; }

private:
	// FF24
	uint8_t SO2Level;
	uint8_t SO1Level;
	// These Vin bits mask the supply of an audio signal from the cartridge, 
	// according to pandocs no games are known to use this feature.
	uint8_t VinToSO2;
	uint8_t VinToSO1;
	uint8_t VolumeRegisterByte;

	// FF25
	bool Chan1ToSO2;
	bool Chan2ToSO2;
	bool Chan3ToSO2;
	bool Chan4ToSO2;
	bool Chan1ToSO1;
	bool Chan2ToSO1;
	bool Chan3ToSO1;
	bool Chan4ToSO1;
	uint8_t ChannelSelectionRegisterByte;

	// FF26
	bool MasterEnable;
	bool Channel1Enable;
	bool Channel2Enable;
	bool Channel3Enable;
	bool Channel4Enable;
};