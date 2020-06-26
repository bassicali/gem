
#pragma once

#include <cstdint>

enum class SpeedMode
{
	Normal = 0x0,
	Double = 0x1,
};

class CGBRegisters
{
	public:
		CGBRegisters();
		uint8_t ReadSpeedRegister() const;
		bool GetPrepareSpeedSwitch() const { return prepareSpeedSwitch; }
		void WriteSpeedRegister(uint8_t value);
		void WriteWorkingRamBank(uint8_t value);
		uint16_t GetWorkingRamBank() const { return wramBank; }
		uint16_t GetWorkingRamOffset() const { return wramOffset; }
		void ToggleSpeed();
		const SpeedMode Speed() const { return currentSpeed;  }
	private:
		int wramBank;
		uint16_t wramOffset;
		bool prepareSpeedSwitch;
		SpeedMode currentSpeed;
};