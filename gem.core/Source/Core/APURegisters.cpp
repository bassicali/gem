
#include <cassert>
#include <cmath>

#include "Logging.h"
#include "Core/GemConstants.h"
#include "Core/APURegisters.h"

////////////////////////////
///   Control Register   ///
////////////////////////////

APUControlRegisters::APUControlRegisters()
	: VinToSO2(false)
	, SO2Level(0)
	, VinToSO1(false)
	, SO1Level(0)
	, VolumeRegisterByte(0)
	, Chan1ToSO2(false)
	, Chan2ToSO2(false)
	, Chan3ToSO2(false)
	, Chan4ToSO2(false)
	, Chan1ToSO1(false)
	, Chan2ToSO1(false)
	, Chan3ToSO1(false)
	, Chan4ToSO1(false)
	, ChannelSelectionRegisterByte(0)
	, MasterEnable(0)
	, Channel1Enable(false)
	, Channel2Enable(false)
	, Channel3Enable(false)
	, Channel4Enable(false)
{
}

void APUControlRegisters::WriteVolumeRegister(const uint8_t value)
{
	SO2Level = (value & 0x70) >> 4;
	SO1Level = (value & 0x7);
	VinToSO2 = uint8_t((value & 0x80) == 0x80);
	VinToSO1 = uint8_t((value & 0x8) == 0x8);
	VolumeRegisterByte = value;
}

uint8_t APUControlRegisters::ReadVolumeRegister() const
{
	return VolumeRegisterByte;
}

void APUControlRegisters::WriteChannelSelectionRegister(const uint8_t value)
{
	Chan1ToSO2 = (value & 0x80) >> 7;
	Chan2ToSO2 = (value & 0x40) >> 6;
	Chan3ToSO2 = (value & 0x20) >> 5;
	Chan4ToSO2 = (value & 0x10) >> 4;
	Chan1ToSO1 = (value & 0x08) >> 3;
	Chan2ToSO1 = (value & 0x04) >> 2;
	Chan3ToSO1 = (value & 0x02) >> 1;
	Chan4ToSO1 = (value & 0x01) >> 0;
	ChannelSelectionRegisterByte = value;
}

uint8_t APUControlRegisters::ReadChannelSelectionRegister() const
{
	return ChannelSelectionRegisterByte;
}

void APUControlRegisters::WriteChannelEnableRegister(const uint8_t value)
{
	MasterEnable = (value & 0x80) == 0x80;
}

uint8_t APUControlRegisters::ReadChannelEnableRegister() const
{
	return (MasterEnable << 7) 
			| (Channel4Enable << 3) 
			| (Channel3Enable << 2) 
			| (Channel2Enable << 1) 
			| Channel1Enable;
}

void APUControlRegisters::GetLeftLevelMask(uint8_t* arr) const
{
	arr[0] = Chan1ToSO2 * SO2Level;
	arr[1] = Chan2ToSO2 * SO2Level;
	arr[2] = Chan3ToSO2 * SO2Level;
	arr[3] = Chan4ToSO2 * SO2Level;
}

void APUControlRegisters::GetRightLevelMask(uint8_t* arr) const
{
	arr[0] = Chan1ToSO1 * SO1Level;
	arr[1] = Chan2ToSO1 * SO1Level;
	arr[2] = Chan3ToSO1 * SO1Level;
	arr[3] = Chan4ToSO1 * SO1Level;
}

////////////////////////////
/// Channel 1 Tone+Sweep ///
////////////////////////////

Channel1Registers::Channel1Registers()
	: sweepPeriod(0)
	, decrease(false)
	, sweepRegisterByte(0)
{
}

void Channel1Registers::StartEmitter()
{
	bool dac_on = (volumeEnvelopeRegisterByte & 0xF8) != 0;
	emitter->Reset(periodData, sweepPeriod, decrease, freqDivider, waveDutyCode, initialVolume, volumeIncrease, envelopePeriod, dac_on, soundLength, stopAfterLen);
}

uint8_t Channel1Registers::ReadSweepRegister() const
{
	return sweepRegisterByte;
}

void Channel1Registers::WriteSweepRegister(const uint8_t value)
{
	sweepPeriod = (value & 0x70) >> 4;
	decrease = (value & 0x8) == 0x8;
	freqDivider = value & 0x7;
	sweepRegisterByte = value;

	emitter->sweepTicksPerShift = sweepPeriod;
	emitter->freqDivider = freqDivider;
	emitter->freqDecreasing = decrease;

	LOG_DEBUG("(%04X) -> SweepTime = %.2fsec; Decreasing = %d; Freq Divider = %d", value, sweepPeriod, decrease, freqDivider);
}

////////////////////////////
///    Channel 2 Tone    ///
////////////////////////////

Channel2Registers::Channel2Registers()
	: emitter(nullptr)
	, waveDutyCode(0x2)
	, soundLength(0.0f)
	, waveDutyAndLengthRegisterByte(0x80)
	, initialVolume(0)
	, volumeIncrease(false)
	, envelopePeriod(0)
	, volumeEnvelopeRegisterByte(0)
	, periodData(0)
	, startPlaying(false)
	, stopAfterLen(true)
	, freqRegisterByte(0x40)
{
}

void Channel2Registers::StartEmitter()
{
	bool dac_on = (volumeEnvelopeRegisterByte & 0xF8) != 0;
	emitter->Reset(periodData, 0.0f, false, 0, waveDutyCode, initialVolume, volumeIncrease, envelopePeriod, dac_on, soundLength, stopAfterLen);
}

void Channel2Registers::WriteWaveDutyAndLengthRegister(const uint8_t value)
{
	waveDutyCode = (value & 0xC0) >> 6;
	soundLength = value & 0x3F;
	waveDutyAndLengthRegisterByte = value;

	emitter->dutyCycleCode = waveDutyCode;

	LOG_DEBUG("(%04X) -> Duty = %d; Len = %d", value, waveDutyCode, soundLength);
}

uint8_t Channel2Registers::ReadWaveDutyAndLengthRegister() const
{
	return waveDutyAndLengthRegisterByte;
}

void Channel2Registers::WriteVolumeEnvelopeRegister(const uint8_t value)
{
	initialVolume = (value & 0xF0) >> 4;
	volumeIncrease = (value & 0x8) == 0x8;
	envelopePeriod = value & 0x7;
	volumeEnvelopeRegisterByte = value;

	emitter->volumeIncreasing = volumeIncrease;
	emitter->envelopeTicksPerShift = envelopePeriod;
	emitter->dacOn = (volumeEnvelopeRegisterByte & 0xF8) != 0;

	LOG_DEBUG("(%04X) -> Initial Volume = %d; Increasing = %d; NumEnvelopes = %d", value, initialVolume, volumeIncrease, envelopePeriod);
}

uint8_t Channel2Registers::ReadVolumeEnvelopeRegister() const
{
	return volumeEnvelopeRegisterByte;
}

void Channel2Registers::WriteFrequencyRegisterHigh(const uint8_t value)
{
	startPlaying = (value & 0x80) == 0x80;
	stopAfterLen = (value & 0x40) == 0x40;
	periodData = ((value & 0x7) << 8) | (periodData & 0xFF);
	emitter->SetWavePeriodData(periodData);
	freqRegisterByte = value & 0x40;
}

void Channel2Registers::WriteFrequencyRegisterLow(const uint8_t value)
{
	periodData = (periodData & 0x700) | value;
	emitter->SetWavePeriodData(periodData);
}

uint8_t Channel2Registers::ReadFrequencyRegister() const
{
	return freqRegisterByte;
}

////////////////////////////
///   Channel 3 Waves    ///
////////////////////////////

Channel3Registers::Channel3Registers()
	: emitter(nullptr)
	, startPlaying(false)
	, channelOn(false)
{

}

void Channel3Registers::WriteOnOffRegister(const uint8_t value)
{
	channelOn = (value & 0x80) != 0;
	emitter->dacOn = channelOn;
}

uint8_t Channel3Registers::ReadOnOffRegister() const
{
	return channelOn ? 0x80 : 0;
}

void Channel3Registers::WriteSoundLengthRegister(const uint8_t value)
{
	soundLength = value;
}

uint8_t Channel3Registers::ReadSoundLengthRegister() const
{
	return soundLength;
}

void Channel3Registers::WriteSoundLevelRegister(const uint8_t value)
{
	amplitudeDivider = (value & 0x60) >> 5;
	shiftRegisterByte = value & 0x60;
}

uint8_t Channel3Registers::ReadSoundLevelRegister() const
{
	return shiftRegisterByte;
}

void Channel3Registers::WriteFrequencyRegisterHigh(const uint8_t value)
{
	startPlaying = (value & 0x80) == 0x80;
	stopAfterLen = (value & 0x40) == 0x40;
	periodData = ((value & 0x7) << 8) | (periodData & 0xFF);
	emitter->SetWavePeriodData(periodData);
	freqRegisterByte = value & 0x40;
}

void Channel3Registers::WriteFrequencyRegisterLow(const uint8_t value)
{
	periodData = (periodData & 0x700) | value;
	emitter->SetWavePeriodData(periodData);
}

uint8_t Channel3Registers::ReadFrequencyRegister() const
{
	return freqRegisterByte;
}

void Channel3Registers::StartEmitter()
{
	emitter->Reset(periodData, soundLength, stopAfterLen, amplitudeDivider, channelOn);
}

////////////////////////////
///   Channel 4 Waves    ///
////////////////////////////

Channel4Registers::Channel4Registers()
	: emitter(nullptr)
	, startPlaying(false)
{
}

void Channel4Registers::WriteSoundLengthRegister(const uint8_t value)
{
	soundLength = value & 0x3F;
	//emitter->soundLenCtr;
}

uint8_t Channel4Registers::ReadSoundLengthRegister() const
{
	return soundLength;
}

void Channel4Registers::WriteVolumeEnvelopeRegister(const uint8_t value)
{
	initialVolume = (value & 0xF0) >> 4;
	volumeIncrease = (value & 0x8) == 0x8;
	envelopePeriod = value & 0x7;
	volumeEnvelopeRegisterByte = value;

	emitter->volumeIncreasing = volumeIncrease;
	emitter->envelopeTicksPerShift = envelopePeriod;
	emitter->dacOn = (volumeEnvelopeRegisterByte & 0xF8) != 0;

	LOG_DEBUG("(%04X) -> Initial Volume = %d; Increasing = %d; NumEnvelopes = %d", value, initialVolume, volumeIncrease, envelopePeriod);
}

uint8_t Channel4Registers::ReadVolumeEnvelopeRegister() const
{
	return volumeEnvelopeRegisterByte;
}

void Channel4Registers::WritePolynomialRegister(const uint8_t value)
{
	shiftClockMultiplier = (value & 0xF0) >> 4;
	shortMode = (value & 0x08) == 0x08;
	periodData = value & 0x07;
	polynomialRegisterByte = value;

	emitter->shortMode = shortMode;
	emitter->SetShiftPeriodData(periodData, shiftClockMultiplier);
}

uint8_t Channel4Registers::ReadPolynomialRegister()
{
	return polynomialRegisterByte;
}

void Channel4Registers::WriteTriggerRegister(const uint8_t value)
{
	startPlaying = (value & 0x80) == 0x80;
	triggerRegisterByte = value & 0x40;
	stopAfterLen = triggerRegisterByte != 0;
}

uint8_t Channel4Registers::ReadTriggerRegister()
{
	return triggerRegisterByte;
}

void Channel4Registers::StartEmitter()
{
	bool dac_on = (volumeEnvelopeRegisterByte & 0xF8) != 0;
	emitter->Reset(periodData, shiftClockMultiplier, shortMode, initialVolume, volumeIncrease, envelopePeriod, dac_on, soundLength, stopAfterLen);
}