#pragma once

#include <functional>
#include <vector>

#include "DArray.h"
#include "IMappedComponent.h"

#include "Core/GPU.h"
#include "Core/APU.h"
#include "Core/CGBRegisters.h"
#include "Core/CartridgeReader.h"
#include "Core/Timers.h"
#include "Core/InterruptController.h"
#include "Core/MBC.h"
#include "Core/Serial.h"
#include "Core/Joypad.h"
#include "Disassembler.h"

class MMU : public IMappedComponent, public std::enable_shared_from_this<MMU>
{
	public:
		MMU();
		void Reset(bool bCGB);
		bool SetCartridge(std::shared_ptr<CartridgeReader> ptr);
		void SaveGame();
		void Shutdown();

		virtual uint8_t ReadByte(uint16_t addr) override;
		uint16_t ReadWord(uint16_t addr);

		virtual void WriteByte(uint16_t addr, uint8_t value) override;
		void WriteWord(uint16_t addr, uint16_t value);

		void WriteByteWorkingRam(uint16_t addr, bool bank0, uint8_t value);
		uint8_t ReadByteWorkingRam(uint16_t addr, bool bank0);

		void SetReadBreakpoints(std::vector<Breakpoint>& bps) { readBreakpoints = &bps; }
		void SetWriteBreakpoints(std::vector<Breakpoint>& bps) { writeBreakpoints = &bps; }
		void EvalBreakpoints(bool eval) { evalBreakpoints = eval; }
		
		std::shared_ptr<CartridgeReader> GetCartridgeReader() { return cart; }
		
		void SetGPU(std::shared_ptr<GPU> ptr);
		void SetAPU(std::shared_ptr<APU> ptr);
		void SetJoypad(std::shared_ptr<Joypad> ptr);

		std::shared_ptr<InterruptController> GetInterruptController() { return interrupts; } 
		TimerController& GetTimerController() { return timer; }
		CGBRegisters& GetCGBRegisters() { return cgb_state; }
		const bool IsCGB() const { return bCGB; }

		// 4kB banks * 8 banks (0-7)
		static const int WRAMBankSize = 0x1000;
		static const int WRAMBanks = 8;
		static const int WRAMSize = 8 * 0x1000;

	private:
		bool bCGB;

		std::vector<Breakpoint>* writeBreakpoints;
		std::vector<Breakpoint>* readBreakpoints;
		bool evalBreakpoints;

		MBC mbc;
		CGBRegisters cgb_state;
		TimerController timer;
		SerialController serial;

		std::shared_ptr<InterruptController> interrupts;
		std::shared_ptr<CartridgeReader> cart;
		std::shared_ptr<GPU> gpu;
		std::shared_ptr<APU> apu;
		std::shared_ptr<Joypad> joypad;

		DArray<uint8_t> wramBanks[8];
		uint8_t hram[128];
};
