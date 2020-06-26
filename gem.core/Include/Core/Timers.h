#pragma once

#include <cstdint>
#include <memory>

#include "Core/InterruptController.h"

class TimerController
{
public:
	uint8_t Divider; // Constantly increments at 16,384Hz // DIV, 0xFF04
	uint8_t Counter; // Variable clock // TIMA, 0xFF05
	uint8_t Modulo; // Counter resets to this when it overflows // TMA, 0xFF06
	bool Running; // Enable/Disable the Counter // TAC, 0xFF07 Bit 2
	uint8_t Speed; // Controls frequency of the Counter // TAC, 0xFF07 Bit 1-0
	uint8_t TACRegisterByte;

	// Timer needs to be able to request interrupts
	void SetInterruptController(std::shared_ptr<InterruptController> ptr);
	
	TimerController();
	void Reset(bool bCGB);
	void WriteByte(uint16_t addr, uint8_t value);
	uint8_t ReadByte(uint16_t addr) const;
	void TickTimers(int t_cycles);
	int GetCounterFrequency();

private:
	std::shared_ptr<InterruptController> interrupts;

	bool bCGB;
	int divAcc; // Accumulates T cycles for the Divider
	int ctrAcc; // Accumulates T cycles for the Counter
	uint32_t tCyclesPerCtrCycle; // Counts how many M cycles fit inside a Counter register period
};
