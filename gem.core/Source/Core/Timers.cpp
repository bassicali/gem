#include <memory>

#include "Core/Timers.h"
#include "Core/InterruptController.h"

using namespace std;

TimerController::TimerController()
{
	Reset(true);
}

void TimerController::Reset(bool bCGB)
{
	this->bCGB = bCGB;

	if (bCGB)
		Divider = 0x1E;
	else
		Divider = 0xAB;

	Counter = 0;
	Modulo = 0;
	Running = false;
	Speed = 0;
	divAcc = 0;
	ctrAcc = 0;
	tCyclesPerCtrCycle = 0;
}

void TimerController::SetInterruptController(shared_ptr<InterruptController> ptr)
{
	interrupts = ptr;
}

void TimerController::WriteByte(uint16_t addr, uint8_t value)
{
	switch (addr)
	{
		case 0xFF04:
			Divider = 0;
			break;
		case 0xFF05:
			Counter = value;
			break;
		case 0xFF06:
			Modulo = value;
			break;
		case 0xFF07:
			Running = (value & 0x4) == 0x4;
			Speed = value & 0x3;
			switch (Speed)
			{
				case 0x0: tCyclesPerCtrCycle = 1024; break; // 4096 Hz
				case 0x1: tCyclesPerCtrCycle = 16; break;   // 262144 Hz
				case 0x2: tCyclesPerCtrCycle = 64; break;  // 65536 Hz
				case 0x3: tCyclesPerCtrCycle = 256; break;  // 16384 Hz
			}

			TACRegisterByte = value;
			break;
		default:
			break;
	}
}

uint8_t TimerController::ReadByte(uint16_t addr) const
{
	switch (addr)
	{
		case 0xFF04:
			return Divider;
		case 0xFF05:
			return Counter;
		case 0xFF06:
			return Modulo;
		case 0xFF07:
			return TACRegisterByte;
		default:
			throw exception("Illegal read from timer controller");
	}
}

/*
T clock = 4,194,304 Hz
M clock = 1,048,576 Hz

Timer's base clock = 262,144 Hz
The Divider and Counter clocks are connected to the base clock
through a series of flip-flops that divide the frequency by powers of 2.

Divider clock = 16,384 Hz (base / 16)
Counter clock = 262,144 or 65,536 or 16,384 or 4096 (base / 1, 4, 16, 64)
*/
void TimerController::TickTimers(int t_cycles)
{
	if (t_cycles == 0) return;

	divAcc += t_cycles;

	// 64 M cycles increments Divider by 1
	if (divAcc >= 256)
	{
		Divider++;
		divAcc -= 256; 
	}

	if (!Running) return;

	ctrAcc += t_cycles;

	while (ctrAcc >= tCyclesPerCtrCycle)
	{
		if (Counter == 0xFF)
		{
			interrupts->TimerRequested = true;
			Counter = Modulo;
		}
		else
		{
			Counter++;
		}

		ctrAcc -= tCyclesPerCtrCycle;
	}
}

int TimerController::GetCounterFrequency()
{
	if (Speed == 0)
		return 4096;
	else if (Speed == 1)
		return 262144;
	else if (Speed == 2)
		return 65536;
	else
		return 16384;
}