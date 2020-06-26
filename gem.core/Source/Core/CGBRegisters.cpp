
#include "Core/CGBRegisters.h"
#include "Logging.h"

CGBRegisters::CGBRegisters() :
	wramBank(1),
	wramOffset(1 * 0xFFF),
	prepareSpeedSwitch(false),
	currentSpeed(SpeedMode::Normal)
{

}

uint8_t CGBRegisters::ReadSpeedRegister() const
{
	return (static_cast<uint8_t>(currentSpeed) << 7) | prepareSpeedSwitch;
}

void CGBRegisters::WriteSpeedRegister(uint8_t value)
{
	prepareSpeedSwitch = (value & 0x1) == 1;
}

void CGBRegisters::WriteWorkingRamBank(uint8_t value)
{
	wramBank = value & 0x7;
	if (wramBank == 0) wramBank = 1;
	wramOffset = wramBank * 0xFFF;
}

void CGBRegisters::ToggleSpeed()
{
	currentSpeed = currentSpeed == SpeedMode::Normal
					? SpeedMode::Double : SpeedMode::Normal;

	LOG_INFO("Speed mode: %s", currentSpeed == SpeedMode::Normal ? "Normal" : "Double");
}
