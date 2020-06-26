#include "Core/InterruptController.h"


InterruptController::InterruptController()
{
	Reset();
}

void InterruptController::Reset()
{
	VBlankEnabled = false;
	LCDStatusEnabled = false;
	TimerEnabled = false;
	SerialEnabled = false;
	JoypadEnabled = false;
	EnableRegisterByte = 0;

	VBlankRequested = true;
	LCDStatusRequested = false;
	TimerRequested = false;
	SerialRequested = false;
	JoypadRequested = false;
}

uint8_t InterruptController::ReadEnabledInterrupts() const
{
	return EnableRegisterByte;
}

uint8_t InterruptController::ReadRequestedInterrupts() const
{
	return (JoypadRequested << 4) | (SerialRequested << 3) | (TimerRequested << 2) | (LCDStatusRequested << 1) | VBlankRequested;
}

void InterruptController::WriteToEnableInterrupts(uint8_t value)
{
	VBlankEnabled = (value & 0x1) == 0x1;
	LCDStatusEnabled = (value & 0x2) == 0x2;
	TimerEnabled = (value & 0x4) == 0x4;
	SerialEnabled = (value & 0x8) == 0x8;
	JoypadEnabled = (value & 0x10) == 0x10;
	EnableRegisterByte = value;
}

void InterruptController::WriteToRequestInterrupts(uint8_t value)
{
	VBlankRequested = (value & 0x1) == 0x1;
	LCDStatusRequested = (value & 0x2) == 0x2;
	TimerRequested = (value & 0x4) == 0x4;
	SerialRequested = (value & 0x8) == 0x8;
	JoypadRequested = (value & 0x10) == 0x10;
}