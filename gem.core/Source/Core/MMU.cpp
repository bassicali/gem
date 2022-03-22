
#include <memory>
#include <cassert>

#include "Core/MMU.h"
#include "Logging.h"

using namespace std;

MMU::MMU() 
	: interrupts(new InterruptController())
	, evalBreakpoints(false)
	, readBreakpoints(nullptr)
	, writeBreakpoints(nullptr)
{
	timer.SetInterruptController(interrupts);
	memset(hram, 0, 127);

	for (int i = 0; i < 8; i++)
		wramBanks[i] = DArray<uint8_t>(WRAMBankSize);
}

void MMU::Reset(bool bCGB)
{
	this->bCGB = bCGB;
	memset(hram, 0, 127);

	for (int i = 0; i < 8; i++)
	{
		if (wramBanks[i].IsAllocated())
			wramBanks[i].Fill(0);
	}

	mbc.Reset();
}

void MMU::Shutdown()
{
	if (cart && cart->Properties().ExtRamHasBattery)
	{
		mbc.WriteSaveGame();
	}
}

bool MMU::SetCartridge(std::shared_ptr<CartridgeReader> ptr)
{
	if (cart)
		cart.reset();

	cart = ptr;

	if (!mbc.IsCartridgeTypeSupported(cart->Properties().Type))
	{
		LOG_ERROR("Gem does not support this cartridge type yet.");
		return false;
	}

	mbc.SetCartridge(cart);
	return true;
}

void MMU::SaveGame()
{
	if (cart && cart->Properties().ExtRamHasBattery)
		mbc.WriteSaveGame();
}

void MMU::SetGPU(std::shared_ptr<GPU> ptr)
{
	gpu = ptr;
}

void MMU::SetAPU(std::shared_ptr<APU> ptr)
{
	apu = ptr;
}

void MMU::SetJoypad(std::shared_ptr<Joypad> ptr)
{
	joypad = ptr;
}

void MMU::WriteByteWorkingRam(uint16_t addr, bool bank0, uint8_t value)
{
	int bank_num = 0;
	if (!bank0)
		bank_num = bCGB ? cgb_state.GetWorkingRamBank() : 1;

	if (bank_num >= WRAMBanks)
		throw exception("Working RAM bank index is too large");

	DArray<uint8_t>& bank = wramBanks[bank_num];
	if (!bank.IsAllocated())
	{
		bank.Allocate();
		bank.Reserve();
	}

	bank[addr & 0xFFF] = value;
}

uint8_t MMU::ReadByteWorkingRam(uint16_t addr, bool bank0)
{
	int bank_num = 0;
	if (bank0 == false)
		bank_num = bCGB ? cgb_state.GetWorkingRamBank() : 1;

	if (bank_num >= WRAMBanks)
		throw exception("Working RAM bank index is too large");

	DArray<uint8_t>& bank = wramBanks[bank_num];
	if (!bank.IsAllocated())
	{
		// Allocation probably wouldn't happen on a read, but just in case...
		bank.Allocate();
		bank.Reserve();
	}

	return bank[addr & 0xFFF];
}

uint8_t MMU::ReadByte(uint16_t addr)
{
	uint8_t ret = 0;

	switch (addr & 0xF000)
	{
		// ROM bank 0 (16384 bytes)
		case 0x0000:
		case 0x1000:
		case 0x2000:
		case 0x3000:
		{
			ret = cart->ReadByte(addr);
			break;
		}

		// ROM bank 1 (switchable) (16384 bytes)
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
		// External RAM (8192 bytes)
		case 0xA000:
		case 0xB000:
		{
			ret = mbc.ReadByte(addr);
			break;
		}

		// GPU
		case 0x8000:
		case 0x9000:
		{
			ret = gpu->ReadByteVRAM(addr);
			break;
		}

		// Working RAM bank 0 (4096 bytes). This bank is always mapped into view.
		case 0xC000:
		{
			ret = ReadByteWorkingRam(addr, true);
			break;
		}

		// Working RAM bank 1-7 (4096 bytes). These banks can be selected via the register at FF70.
		case 0xD000:
		{
			ret = ReadByteWorkingRam(addr, false);
			break;
		}

		case 0xE000:
		case 0xF000:
		{
			// Working RAM shadow
			// Reading from E000..FDFF is equivalent to reading from C000..DDFF. So we just subtract 0x2000 from the address.
			if (addr < 0xFE00)
			{
				ret = ReadByte(addr - 0x2000);
				break;
			}

			// Everything from here is memory mapped IO and various control registers
			switch (addr & 0x0F00)
			{
				case 0xE00:
				{
					if (addr < 0xFEA0)
						ret = gpu->ReadByteOAM(addr); // FE00-FEDF
					else
						LOG_VERBOSE("[MMU] Memory range FEA0-FEFF is unusable");

					break;
				}
				case 0xF00:
				{
					switch (addr & 0x00F0)
					{
						case 0x00:
						{
							if (addr == 0xFF00)
								ret = joypad->ReadByte();
							else if (addr == 0xFF01)
								ret = serial.TxData;
							else if (addr == 0xFF02)
								ret = serial.ReadByte();
							else if (addr >= 0xFF04 && addr <= 0xFF07)
							{
								ret = timer.ReadByte(addr);
							}
							else if (addr == 0xFF0F)
								ret = interrupts->ReadRequestedInterrupts();

							break;
						}
						case 0x10:
						case 0x20:
						{
							ret = apu->ReadRegister(addr);
							break;
						}
						case 0x30:
						{
							if (addr <= 0xFF3F)
								ret = apu->ReadWaveRAM(addr);
							else
								LOG_VERBOSE("[MMU] ReadByte: Illegal or unsupported address: %Xh", addr);

							break;
						}
						case 0x40:
						case 0x50:
						case 0x60:
						{
							if (addr == 0xFF4D)
								ret = cgb_state.ReadSpeedRegister();
							else
								ret = gpu->ReadRegister(addr);

							break;
						}
						case 0x70:
						{
							if (addr == 0xFF70)
							{
								if (!bCGB)
									LOG_VIOLATION("[MMU] FF70 cannot be used in non CGB mode");

								ret = uint8_t(cgb_state.GetWorkingRamBank());
							}

							break;
						}

						case 0x80:
						case 0x90:
						case 0xA0:
						case 0xB0:
						case 0xC0:
						case 0xD0:
						case 0xE0:
						case 0xF0:
						{
							if (addr <= 0xFFFE)
								ret = hram[addr & 0x7F]; // High ram: FF80h-FFFEh

							if (addr == 0xFFFF)
								ret = interrupts->ReadEnabledInterrupts();

							break;
						}
					}

					break;
				}
				default:
					LOG_VERBOSE("[MMU] ReadByte: Illegal or unsupported address: %Xh", addr);
					break;
			}
			break;
		}
		default:
			break;
	}

	if (evalBreakpoints && readBreakpoints)
	{
		for (auto& bp : *readBreakpoints)
		{
			if (addr == bp.Address)
			{
				if (bp.CheckValue)
					bp.Hit = bp.ValueIsMask ? ((ret & bp.Value) != 0) : (ret == bp.Value);
				else
					bp.Hit = true;
			}
		}
	}

	return ret;
}

uint16_t MMU::ReadWord(uint16_t addr) 
{
	uint16_t cat = ReadByte(addr) + (ReadByte(addr + 0x1) << 8);
	return cat;
}

void MMU::WriteByte(uint16_t addr, uint8_t value)
{
	static string device_name; // Used to log a message at the end of this function

	switch (addr & 0xF000)
	{
		// MBC controllers
		case 0x0000:
		case 0x1000:
		case 0x2000:
		case 0x3000:
		case 0x4000:
		case 0x6000:
		case 0x7000:
		case 0xA000:
		case 0xB000:
		{
			mbc.WriteByte(addr, value);
			device_name = "MBC1";
			break;
		}

		// GPU
		case 0x8000:
		case 0x9000:
		{
			gpu->WriteByteVRAM(addr, value);
			device_name = "VRAM";
			break;
		}

		// Working RAM bank 0 (4096 bytes)
		case 0xC000:
		{
			WriteByteWorkingRam(addr, true, value);
			device_name = "WRAM";
			break;
		}

		// Working RAM bank 1-7 (4096 bytes)
		case 0xD000:
		{
			WriteByteWorkingRam(addr, false, value);
			device_name = "WRAM";
			break;
		}

		case 0xE000:
		case 0xF000:
		{
			// Working RAM shadow
			// Writing to E000..FDFF is equivalent to reading from C000..DDFF. So we just subtract 0x2000 from the address.
			if (addr < 0xFE00)
				return WriteByte(addr - 0x2000, value);

			// Everything from here is memory mapped IO and various control registers
			switch (addr & 0x0F00)
			{
				case 0xE00:
				{
					if (addr < 0xFEA0)
						gpu->WriteByteOAM(addr, value); // FE00-FEDF
					else
						LOG_VIOLATION("[MMU] Memory range FEA0-FEFF is unusable");

					device_name = "OAM";
					break;
				}
				case 0xF00: // 0xFFXX
				{
					switch (addr & 0x00F0)
					{
						case 0x00:
						{
							if (addr == 0xFF00)
								joypad->WriteByte(value);
							else if (addr == 0xFF01)
								serial.TxData = value;
							else if (addr == 0xFF02)
								serial.WriteByte(value);
							else if (addr >= 0xFF04 && addr <= 0xFF07)
								timer.WriteByte(addr, value);
							else if (addr == 0xFF0F)
								interrupts->WriteToRequestInterrupts(value);

							break;
						}
						case 0x10:
						case 0x20:
						{
							apu->WriteRegister(addr, value);
							break;
						}
						case 0x30:
						{
							if (addr <= 0xFF3F)
								apu->WriteWaveRAM(addr, value);
							else
								LOG_VERBOSE("[MMU] WriteByte: Illegal or unsupported address: %Xh", addr);

							break;
						}
						case 0x40:
						case 0x50:
						case 0x60:
						{
							if (addr == 0xFF4D)
							{
								cgb_state.WriteSpeedRegister(value);
							}
							else
							{
								gpu->WriteRegister(addr, value);
							}

							break;
						}
						case 0x70:
						{
							if (addr == 0xFF70)
							{
								if (!bCGB)
									LOG_VIOLATION("[MMU] FF70 cannot be used in non CGB mode.");

								cgb_state.WriteWorkingRamBank(value);
							}

							break;
						}
						case 0x80:
						case 0x90:
						case 0xA0:
						case 0xB0:
						case 0xC0:
						case 0xD0:
						case 0xE0:
						case 0xF0:
						{
							if (addr <= 0xFFFF)
								hram[addr & 0x7F] = value;

							if (addr == 0xFFFF)
								interrupts->WriteToEnableInterrupts(value);

							break;
						}
					}

					device_name = "REG";
					break;
				}
				default:
					LOG_VERBOSE("[MMU] WriteByte: Illegal or unsupported address: %Xh", addr);
					break;
			}

			break;
		}

		default:
			LOG_VERBOSE("[MMU] WriteByte: Illegal or unsupported address: %Xh", addr);
			break;
	}

	LOG_VERBOSE("[MMU] %s[%Xh] = %d", device_name.c_str(), addr, value);

	if (evalBreakpoints && writeBreakpoints)
	{
		for (auto& bp : *writeBreakpoints)
		{
			if (addr == bp.Address)
			{
				if (bp.CheckValue)
					bp.Hit = bp.ValueIsMask ? ((value & bp.Value) != 0) : (value == bp.Value);
				else
					bp.Hit = true;
			}
		}
	}
}

void MMU::WriteWord(uint16_t addr, uint16_t value)
{
	WriteByte(addr, value & 0xFF);
	WriteByte(addr + 1, (value & 0xFF00) >> 8);
}
