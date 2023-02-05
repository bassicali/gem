
#include <fstream>
#include <ctime>
#include <cassert>

#include "Core/MBC.h"
#include "Core/CartridgeReader.h"
#include "Logging.h"

using namespace std;

MBC::MBC()
	: cart(nullptr)
{
	lastLatchedTime = {0};
	Reset();

	extRAMBanks[0] = DArray<uint8_t>(RAMBankSize);
	extRAMBanks[1] = DArray<uint8_t>(RAMBankSize);
	extRAMBanks[2] = DArray<uint8_t>(RAMBankSize);
	extRAMBanks[3] = DArray<uint8_t>(RAMBankSize);
}

void MBC::Reset()
{
	bankingMode = BankingMode::RomMode;
	romBank = 1;
	romOffset = romBank * ROMBankSize;
	extRAMBank = 0;
	extRAMOffset = extRAMBank * RAMBankSize;
	exRAMEnabled = false;
	latchedRTCRegister = -1;
	zeroSeen = false;
	rtcEnabled = false;
	daysOverflowed = false;

	rtcRegisters[0] = 0;
	rtcRegisters[1] = 0;
	rtcRegisters[2] = 0;
	rtcRegisters[3] = 0;
	rtcRegisters[4] = 0;
}

void MBC::SetCartridge(std::shared_ptr<CartridgeReader> ptr)
{
	if (cart && cp.NumRAMBanks > 0)
	{
		for (int i = 0; i < cp.NumRAMBanks; i++)
			extRAMBanks[i].Fill(0);

		cart.reset();
	}

	cart = ptr;
	cp = ptr->Properties();

	assert(cp.NumRAMBanks >= 0 && cp.NumRAMBanks <= 4);

	if (cp.NumRAMBanks > 0)
	{
		for (int i = 0; i < cp.NumRAMBanks; i++)
		{
			if (!extRAMBanks[i].IsAllocated())
			{
				extRAMBanks[i].Allocate();
				extRAMBanks[i].Reserve();
			}
		}
	}
}

bool MBC::SaveExternalRAM(ofstream& fout, streamsize& write_count)
{
#define WRITE(ptr, size) fout.write(reinterpret_cast<char*>(ptr), size); if (fout.good()) { write_count += size; } else { LOG_ERROR("Unexpected error writing save game file"); return false; }
	if (!cp.ExtRamHasBattery)
	{
		return true;
	}

	uint8_t curr = cp.HasRTC ? 1 : 0;
	WRITE(&curr, 1)
	WRITE(&lastLatchedTime, sizeof(tm));

	curr = cp.NumRAMBanks;
	WRITE(&curr, 1)

	for (int i = 0; i < cp.NumRAMBanks; i++)
	{
		assert(extRAMBanks[i].IsAllocated());
		WRITE(extRAMBanks[i].Ptr(), extRAMBanks[i].Size());
	}

	return true;
#undef WRITE
}

bool MBC::LoadExternalRAM(ifstream& input, streamsize& read_count)
{
#define READ(ptr, size) input.read(reinterpret_cast<char*>(ptr), size); if (input.good()) { read_count += size; } else { LOG_ERROR("Save game file is invalid"); return false; }

	if (!cp.ExtRamHasBattery)
	{
		return true;
	}

	uint8_t curr = 0;
	READ(&curr, 1)

	if (curr == 0 && cp.HasRTC) // 0 = No RTC data
	{
		LOG_ERROR("Save game file is invalid (RTC flag)");
		return false;
	}

	READ(&lastLatchedTime, sizeof(tm))

	READ(&curr, 1)
	if (curr != cp.NumRAMBanks)
	{
		LOG_ERROR("Save game file is invalid (RAM size)");
		return false;
	}

	for (int i = 0; i < cp.NumRAMBanks; i++)
	{
		if (!extRAMBanks[i].IsAllocated())
			extRAMBanks[i].Allocate();

		extRAMBanks[i].Reserve();
		READ(extRAMBanks[i].Ptr(), RAMBankSize)
	}

	return true;
#undef READ
}

bool MBC::IsCartridgeTypeSupported(CartridgeType type) const
{
	switch (type)
	{
		case CartridgeType::RomOnly:
		case CartridgeType::MBC1:
		case CartridgeType::MBC1_ExRAM:
		case CartridgeType::MBC1_ExBatteryRAM:
		case CartridgeType::MBC3_Timer_Battery:
		case CartridgeType::MBC3_Timer_ExBatteryRAM:
		case CartridgeType::MBC3:
		case CartridgeType::MBC3_ExRam:
		case CartridgeType::MBC3_ExBatteryRAM:
		case CartridgeType::MBC5:
		case CartridgeType::MBC5_ExRam:
		case CartridgeType::MBC5_ExBatteryRam:
			return true;

		case CartridgeType::MBC2:
		case CartridgeType::MBC2_Battery:
		case CartridgeType::ROM_ExRam:
		case CartridgeType::ROM_ExBatteryRAM:
		case CartridgeType::MMM01:
		case CartridgeType::MMM01_ExRam:
		case CartridgeType::MMM01_ExBatteryRAM:
		case CartridgeType::MBC4:
		case CartridgeType::MBC4_ExRam:
		case CartridgeType::MBC4_ExBatteryRAM:
		case CartridgeType::MBC5_Rumble:
		case CartridgeType::MBC5_Rumble_ExRam:
		case CartridgeType::MBC5_Rumble_ExBatteryRam:
			return false;
	}

	return false;
}

void MBC::UpdateRTCRegisters()
{
	time_t now_time = time(0);
	tm now_tm;
	
	errno_t err = localtime_s(&now_tm, &now_time);
	if (err != 0)
	{
		LOG_ERROR("Failed to fetch current time for RTC update. Err no.: %d", err);
		return;
	}

	time_t last_time = mktime(&lastLatchedTime);
	int delta = now_time - last_time;

	rtcRegisters[0] = delta % 60;
	rtcRegisters[1] = (rtcRegisters[1] + (delta / 60)) % 60;
	rtcRegisters[2] = (rtcRegisters[2] + (delta / 3600)) % 24;

	int days = delta / 86'400;
	if (dayCtr + days > 511)
	{
		daysOverflowed = true;
	}

	dayCtr = (dayCtr + days) % 511;

	rtcRegisters[3] = dayCtr & 0xFF;

	if (daysOverflowed)
		rtcRegisters[4] = rtcRegisters[4] | 0x80;

	if ((dayCtr & 0x100) != 0)
		rtcRegisters[4] = rtcRegisters[4] | 0x1;

	lastLatchedTime = now_tm;
}

uint8_t MBC::ReadByteExtRAM(uint16_t addr)
{
	DArray<uint8_t>& bank = extRAMBanks[extRAMBank];

	if (!bank.IsAllocated())
	{
		bank.Allocate();
		bank.Reserve();
	}

	return bank[addr & 0x1FFF];
}

void MBC::WriteByteExtRAM(uint16_t addr, uint8_t value)
{
	DArray<uint8_t>& bank = extRAMBanks[extRAMBank];

	if (!bank.IsAllocated())
	{
		bank.Allocate();
		bank.Reserve();
	}

	bank[addr & 0x1FFF] = value;
}

uint8_t MBC::ReadByte(uint16_t addr)
{
	switch (addr & 0xF000)
	{
		// ROM bank 1 (switchable) (16384 bytes)
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
		{
			// TODO: check ROM size before trying to read.
			int rom_index = addr & 0x3FFF;

			if (int(cp.Type) > 0) // Type 0 is rom only, no banks
				rom_index += romOffset;
			else
				rom_index += 0x4000; // This region is always readable, even if it's not bankable.

			return cart->ReadByte(rom_index);
		}

		// External RAM (8192 bytes)
		case 0xA000:
		case 0xB000:
		{
			if (cp.Version == MBCVersion::MBC1)
			{
				// TODO: check external ram size before reads or writes
				if (!cp.HasExtRam)
				{
					LOG_ERROR("[MMU] External RAM is not supported by this ROM.");
					return 0;
				}
				else if (!exRAMEnabled)
				{
					LOG_WARN("[MMU] External RAM cannot be accessed when it is disabled.");
					return 0;
				}

				return ReadByteExtRAM(addr);
			}
			else if (cp.Version == MBCVersion::MBC3)
			{
				if (latchedRTCRegister == -1)
				{
					return ReadByteExtRAM(addr);
				}
				else
				{
					if (!rtcEnabled)
						LOG_WARN("RTC register being accessed before being enabled");

					return rtcRegisters[latchedRTCRegister];
				}
			}
			else if (cp.Version == MBCVersion::MBC5)
			{
				return ReadByteExtRAM(addr);
			}
			else
			{
				assert(false);
			}

			break;
		}
	}

	return 0;
}

void MBC::WriteByte(uint16_t addr, uint8_t value)
{
	switch (addr & 0xF000)
	{
		// Control register for enabling external ram. NOTE: this can't be used for a cartridge type that doesn't have external ram
		case 0x0000:
		case 0x1000:
		{
			bool enable = (value & 0x0F) == 0x0A;

			if (cp.HasExtRam)
				exRAMEnabled = enable;

			if (cp.HasRTC)
				rtcEnabled = enable;

			break;
		}

		// Control register for switching ROM banks. Specifically, this sets the lower 5 bits of the ROM bank number.
		// The next 2 higher bits can be set with the next register. This can be used with any MBC cartridge type.
		case 0x2000:
		case 0x3000:
		{
			// MBC1: lower 5 bits of ROM bank #
			// MBC3: whole 7 bits of ROM bank #
			// MBC5: 2000h-2FFFh: lower 8 bits of ROM bank #, 3000h-3FFFh: upper 1 bit

			if (cp.Version == MBCVersion::MBC1)
			{
				uint8_t code = value & 0x1F;
				if (code == 0)
					code = 1;

				romBank = (romBank & 0x60) | code; // Bits 5 and 6 must be left untouched
				LOG_VERBOSE("[GEM] RomBank %d", romBank);
			}
			else if (cp.Version == MBCVersion::MBC3)
			{
				romBank = value & 0x7F;
				if (romBank == 0) romBank = 1;
			}
			else if (cp.Version == MBCVersion::MBC5)
			{
				if (addr < 0x3000)
					romBank = (romBank & 0x0100) | value;
				else
					romBank = ((value & 0x1) << 8) | (romBank & 0xFF);

				// NOTE: MBC5 allows selecting rom bank 0
			}
			else
			{
				assert(false);
			}

			if (romBank >= cp.NumROMBanks)
				romBank = cp.NumROMBanks - 1;

			break;
		}

		// Control register for setting RAM or ROM banks. In ROM mode, this sets bits 5 and 6 of the ROM bank number.
		// In RAM mode it sets the entire 2bit RAM bank number.
		case 0x4000:
		case 0x5000:
		{
			// MBC1: ROM mode: sets bit 5 and bit 6 of ROM bank #
			//		 RAM mode: 2bits of RAM bank # 
			//
			// MBC3: 2 bits of RAM bank #
			//		 unless 0x8-0xC which sets the mapped RTC register
			//
			// MBC5: 8 bits of RAM bank #

			uint8_t code = value & 0x03;

			if (cp.Version == MBCVersion::MBC1)
			{
				if (bankingMode == BankingMode::RamMode)
				{
					extRAMBank = code;
					LOG_VERBOSE("[GEM] RamBank %d", extRAMBank);
				}
				else
				{
					romBank = (romBank & 0x1F) | (code << 5); // Lower 5 bits must be left untouched
					if (romBank == 0x20) romBank = 0x21;
					if (romBank == 0x40) romBank = 0x41;
					if (romBank == 0x60) romBank = 0x61;

					LOG_VERBOSE("[GEM] RomBank %d", romBank);
				}
			}
			else if (cp.Version == MBCVersion::MBC3)
			{
				if (cp.HasRTC && value >= 0x8 && value <= 0xC)
				{
					latchedRTCRegister = value - 0x8;
					LOG_VERBOSE("[GEM] RTC register mapped: %d", value);
				}
				else
				{
					latchedRTCRegister = -1;
					extRAMBank = code;
					LOG_VERBOSE("[GEM] RamBank %d", extRAMBank);
				}
			}
			else if (cp.Version == MBCVersion::MBC5)
			{
				extRAMBank = value & 0xF;
				LOG_VERBOSE("[GEM] RamBank %d", extRAMBank);
			}
			else
			{
				assert(false);
			}

			break;
		}

		// Control register for switching between ROM and RAM mode
		case 0x6000:
		case 0x7000:
		{
			if (cp.Version == MBCVersion::MBC1)
			{
				if (value == 0x00)
					bankingMode = BankingMode::RomMode;
				else if (cp.Type == CartridgeType::MBC1)
					LOG_ERROR("[GEM] Can't use RAM banking mode with this cartridge type.");
				else
					bankingMode = BankingMode::RamMode;
			}
			else if (cp.HasRTC)
			{
				if (!zeroSeen && value == 0)
				{
					zeroSeen = true;
				}
				if (zeroSeen && value != 1)
				{
					zeroSeen = false;
				}
				else if (zeroSeen && value == 1)
				{
					UpdateRTCRegisters();
					zeroSeen = false;
				}
			}

			break;
		}

		case 0xA000:
		case 0xB000:
		{
			if (!cp.HasExtRam)
			{
				LOG_VIOLATION("[MMU] External RAM is not supported by this ROM.");
			}
			else if (!exRAMEnabled)
			{
				LOG_VIOLATION("[MMU] External RAM cannot be accessed when it is disabled.");
			}
			else
			{
				WriteByteExtRAM(addr, value);
			}

			break;
		}

		default:
			LOG_VIOLATION("MBC: Unsupported write (addr: %04X)", addr);
			break;
	}

	romOffset = romBank * ROMBankSize;
	extRAMOffset = extRAMBank * RAMBankSize;
}
