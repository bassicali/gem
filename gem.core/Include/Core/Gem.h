#pragma once

#include <memory>
#include <fstream>

#include "Core/CartridgeReader.h"
#include "Core/Z80.h"
#include "Core/MMU.h"
#include "Core/GPU.h"
#include "Core/APU.h"
#include "Core/Joypad.h"

class Gem
{
	public:
		Gem();
		~Gem();

		void Reset(bool bCGB);
		void Shutdown();
		void LoadRom(const char* file);
		bool IsROMLoaded() const { return cart.get() != nullptr; }
		std::shared_ptr<CartridgeReader> GetCartridgeReader() { return cart; }

		bool Tick();
		void TickUntilVBlank();
		const uint64_t GetTickCount() const { return tickCount; }
		const uint64_t GetFrameCount() const { return frameCount; }
		
		Z80& GetCPU() { return cpu; }
		std::shared_ptr<GPU> GetGPU() { return gpu; }
		std::shared_ptr<APU> GetAPU() { return apu; }
		std::shared_ptr<MMU> GetMMU() { return mmu; }
		std::shared_ptr<Joypad> GetJoypad() { return joypad; }

		void ToggleSound(bool enabled);

		std::string StartTrace();
		void EndTrace();
		void HandleTracing(uint16_t pc, uint16_t inst);

		bool IsCGB() const { return bCGB; }

	private:
		uint64_t tickCount;
		uint64_t frameCount;
		bool bCGB;

		Z80 cpu;
		std::shared_ptr<CartridgeReader> cart;
		std::shared_ptr<MMU> mmu;
		std::shared_ptr<GPU> gpu;
		std::shared_ptr<APU> apu;
		std::shared_ptr<Joypad> joypad;

		bool tickAPU;

		std::ofstream* traceFile;
		bool isTracing;

		friend class RewindManager;
};
