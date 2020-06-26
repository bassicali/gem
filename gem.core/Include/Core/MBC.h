#pragma once

#include <memory>
#include <cstdint>

#include "DArray.h"
#include "Core/CartridgeReader.h"

#define _GEM_SAVE_GAME_HEADER "**GEM_SAVE_GAME**"

enum class BankingMode
{
	RomMode = 0, 
	RamMode = 1,
};

class MBC
{
	public:
		MBC();
		void Reset();
		void SetCartridge(std::shared_ptr<CartridgeReader> ptr);

		uint8_t ReadByte(uint16_t addr);
		void WriteByte(uint16_t addr, uint8_t value);

		uint8_t ReadByteExtRAM(uint16_t addr);
		void WriteByteExtRAM(uint16_t addr, uint8_t value);

		bool LoadSaveGame();
		void WriteSaveGame();

		bool IsCartridgeTypeSupported(CartridgeType type) const;
		int GetROMOffset() const { return romOffset; };
		int GetExternalRAMOffset() const { return extRAMOffset; };
		bool IsExternalRAMEnabled() const { return exRAMEnabled; }

private:
		const int ROMBankSize = 0x4000;
		const int RAMBankSize = 0x2000;

		std::shared_ptr<CartridgeReader> cart;
		CartridgeProperties cp;

		BankingMode bankingMode;
		uint16_t romBank;
		int romOffset;

		bool exRAMEnabled;
		uint16_t extRAMBank;
		int extRAMOffset;
		int numExtRAMBanks; // The currently mapped bank
		DArray<uint8_t> extRAMBanks[4];
		
		// TODO: put RTC in its own class?
		bool rtcEnabled;
		bool zeroSeen; // Tracks the 0x00,0x01 write sequence to latch the RTC registers
		int latchedRTCRegister;
		uint8_t rtcRegisters[5];
		int dayCtr;
		void UpdateRTCRegisters();
		tm lastLatchedTime;
		bool daysOverflowed;
};

