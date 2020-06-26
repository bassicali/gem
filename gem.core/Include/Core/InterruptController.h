#pragma once
#include <cstdint>

struct InterruptController
{
public:
	InterruptController();
	void Reset();
	uint8_t ReadEnabledInterrupts() const;
	uint8_t ReadRequestedInterrupts() const;
	uint8_t ReadPendingInterrupts() const { return ReadEnabledInterrupts() & ReadRequestedInterrupts(); }
	void WriteToEnableInterrupts(uint8_t value);
	void WriteToRequestInterrupts(uint8_t value);

	bool VBlankEnabled;
	bool LCDStatusEnabled;
	bool TimerEnabled;
	bool SerialEnabled;
	bool JoypadEnabled;
	uint8_t EnableRegisterByte;

	bool VBlankRequested;
	bool LCDStatusRequested;
	bool TimerRequested;
	bool SerialRequested;
	bool JoypadRequested;
};