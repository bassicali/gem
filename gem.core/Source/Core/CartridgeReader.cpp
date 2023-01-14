#include <fstream>
#include <cassert>
#include <filesystem>

#include "Logging.h"
#include "Core/CartridgeReader.h"

using namespace std;

CartridgeProperties& CartridgeProperties::operator=(const CartridgeProperties& other)
{
	Type = other.Type;
	Version = other.Version;
	CGBCompatability = other.CGBCompatability;
	HasExtRam = other.HasExtRam;
	ExtRamHasBattery = other.ExtRamHasBattery;
	HasRTC = other.HasRTC;
	NumROMBanks = other.NumROMBanks;
	ROMSize = other.ROMSize;
	NumRAMBanks = other.NumRAMBanks;
	RAMSize = other.RAMSize;

	for (int i = 0; i < 16; i++)
		Title[i] = other.Title[i];

	return *this;
}

CartridgeReader::CartridgeReader()
	: cursor(0)
	, size(0)
	, isLoaded(false)
	, romData(nullptr)
	, saveGameExists(false)
{
}

void CartridgeReader::LoadFile(const char* file)
{
	ifstream fin(file, ios::binary | ios::in);
	if (fin.fail())
		throw exception((string("Unable to open file: ") + string(file)).c_str());

	fin.seekg(0, fin.end);
	int file_size = fin.tellg();
	fin.seekg(0, fin.beg);

	if (romData != nullptr)
	{
		if (size < file_size)
		{
			void* new_ptr = realloc(romData, file_size);
			if (new_ptr == nullptr)
				throw exception("Unable to resize rom data allocation");

			romData = static_cast<uint8_t*>(new_ptr);
		}
	}
	else
	{
		romData = new uint8_t[file_size];
	}

	size = file_size;
	fin.read(reinterpret_cast<char*>(romData), size);
	fin.close();

	DecodeHeader(romData + 0x100, cartProps);

	if (cartProps.ROMSize > size)
	{
		LOG_CONS("ROM size from cartridge header is larger than actual file size");
		cartProps.ROMSize = size;
		cartProps.NumROMBanks = size / 0x4000;
	}

	if (cartProps.ExtRamHasBattery)
	{
		string path(file);
		int pos = path.find_last_of('.');
		if (pos >= 0)
		{
			path = path.substr(0, pos) + ".gem";
		}
		else
		{
			path = path + ".gem";
		}
		saveGameExists = filesystem::exists(path);
		saveGamePath = path;
	}

	cursor = 0;
	isLoaded = true;
}

uint8_t CartridgeReader::ReadByte()
{
	if (cursor >= size)
	{
		throw exception("Reached end of rom file");
	}

	return romData[cursor++];
}

uint8_t CartridgeReader::ReadByte(int index) const
{
	if (index >= size)
	{
		throw exception("Index out of range for rom file");
	}

	return romData[index];
}

CartridgeReader::~CartridgeReader()
{
	if (romData != nullptr)
		delete[] romData;
}

/*static*/
void CartridgeReader::DecodeHeader(const uint8_t header_data[], CartridgeProperties& props)
{
	// NOTE: header_data is offset by 0x100 already
	memcpy((void*)props.Title, header_data + 0x34, 16); // Title in ROM is 0 padded

	uint8_t compatability_flag = header_data[0x43];

	if (compatability_flag == 0x80)
		props.CGBCompatability = CGBSupport::BackwardsCompatible;
	else if (compatability_flag == 0xC0)
		props.CGBCompatability = CGBSupport::CGBOnly;
	else
		props.CGBCompatability = CGBSupport::NoCGBSupport;

	uint8_t rom_size_flag = header_data[0x48];
	if (rom_size_flag <= 7)
	{
		props.ROMSize = 32'768 << rom_size_flag;
		props.NumROMBanks = rom_size_flag > 0
								? (4 << (rom_size_flag - 1)) : 2;
	}
	else if (rom_size_flag == 0x52)
	{
		props.NumROMBanks = 72;
		props.ROMSize = props.NumROMBanks * 0x4000;
	}
	else if (rom_size_flag == 0x53)
	{
		props.NumROMBanks = 80;
		props.ROMSize = props.NumROMBanks * 0x4000;
	}
	else if (rom_size_flag == 0x54)
	{
		props.NumROMBanks = 96;
		props.ROMSize = props.NumROMBanks * 0x4000;
	}

	uint8_t ram_size_flag = header_data[0x49];

	if (ram_size_flag == 0x01)
	{
		props.NumRAMBanks = 1; // 2KB
		props.RAMSize = 2;
	}
	else if (ram_size_flag == 0x02)
	{
		props.NumRAMBanks = 1; // 8KB
		props.RAMSize = 8;
	}
	else if (ram_size_flag == 0x03)
	{
		props.NumRAMBanks = 4; // 32 KB
		props.RAMSize = 32;
	}
	else if (ram_size_flag == 0x10)
	{
		props.NumRAMBanks = 16; // 128 KB
		props.RAMSize = 128;
	}
	else if (ram_size_flag != 0)
	{
		throw exception("Unrecognized RAM size flag");
	}

	props.Type = static_cast<CartridgeType>(header_data[0x47]);

	switch (props.Type)
	{
		case CartridgeType::MBC1_ExRAM:
		case CartridgeType::MBC1_ExBatteryRAM:
			props.HasExtRam = true;
		case CartridgeType::RomOnly:
		case CartridgeType::MBC1:
			props.Version = MBCVersion::MBC1;
			break;

		case CartridgeType::MBC2_Battery:
		case CartridgeType::ROM_ExBatteryRAM:
		case CartridgeType::MMM01_ExBatteryRAM:
			props.ExtRamHasBattery = true;
		case CartridgeType::MBC2:
		case CartridgeType::ROM_ExRam:
		case CartridgeType::MMM01:
		case CartridgeType::MMM01_ExRam:
			props.Version = MBCVersion::MBC2;
			break;

		case CartridgeType::MBC3_Timer_Battery:
		case CartridgeType::MBC3_Timer_ExBatteryRAM:
		case CartridgeType::MBC3_ExBatteryRAM:
			props.ExtRamHasBattery = true;
		case CartridgeType::MBC3:
		case CartridgeType::MBC3_ExRam:
		{
			props.Version = MBCVersion::MBC3;

			props.HasRTC = props.Type == CartridgeType::MBC3_Timer_Battery
							|| props.Type == CartridgeType::MBC3_Timer_ExBatteryRAM;

			props.HasExtRam = props.Type == CartridgeType::MBC3_Timer_ExBatteryRAM
								|| props.Type == CartridgeType::MBC3_ExRam
								|| props.Type == CartridgeType::MBC3_ExBatteryRAM;
			break;
		}

		case CartridgeType::MBC4:
		case CartridgeType::MBC4_ExRam:
		case CartridgeType::MBC4_ExBatteryRAM:
		{
			props.Version = MBCVersion::MBC4;
			props.HasExtRam = props.Type == CartridgeType::MBC4_ExRam
								|| props.Type == CartridgeType::MBC4_ExBatteryRAM;
			break;
		}

		case CartridgeType::MBC5_ExBatteryRam:
		case CartridgeType::MBC5_Rumble_ExBatteryRam:
			props.ExtRamHasBattery = true;
		case CartridgeType::MBC5:
		case CartridgeType::MBC5_ExRam:
		case CartridgeType::MBC5_Rumble:
		case CartridgeType::MBC5_Rumble_ExRam:
		{
			props.Version = MBCVersion::MBC5;
			props.HasExtRam = props.Type == CartridgeType::MBC5_ExRam
								|| props.Type == CartridgeType::MBC5_ExBatteryRam
								|| props.Type == CartridgeType::MBC5_Rumble_ExRam
								|| props.Type == CartridgeType::MBC5_Rumble_ExBatteryRam;
			break;
		}

		default:
			props.Version = MBCVersion::MBC1;
			break;
	}
}

/*static*/
string CartridgeReader::CGBSupportString(CGBSupport supp)
{
	switch (supp)
	{
		case CGBSupport::BackwardsCompatible:
			return "CGB+DMG Compatible";
		case CGBSupport::CGBOnly:
			return "CGB Only";
		case CGBSupport::NoCGBSupport:
			return "DMG Only";
	}

	return "";
}

/*static*/
string CartridgeReader::ROMTypeString(CartridgeType type)
{
	switch (type)
	{
		case CartridgeType::MBC1_ExRAM:
			return "MBC1_ExRAM";
		case CartridgeType::MBC1_ExBatteryRAM:
			return "MBC1_ExBatteryRAM";
		case CartridgeType::RomOnly:
			return "RomOnly";
		case CartridgeType::MBC1:
			return "MBC1";
		case CartridgeType::MBC2_Battery:
			return "MBC2_Battery";
		case CartridgeType::ROM_ExBatteryRAM:
			return "ROM_ExBatteryRAM";
		case CartridgeType::MMM01_ExBatteryRAM:
			return "MMM01_ExBatteryRAM";
		case CartridgeType::MBC2:
			return "MBC2";
		case CartridgeType::ROM_ExRam:
			return "ROM_ExRam";
		case CartridgeType::MMM01:
			return "MMM01";
		case CartridgeType::MMM01_ExRam:
			return "MMM01_ExRam";
		case CartridgeType::MBC3_Timer_Battery:
			return "MBC3_Timer_Battery";
		case CartridgeType::MBC3_Timer_ExBatteryRAM:
			return "MBC3_Timer_ExBatteryRAM";
		case CartridgeType::MBC3_ExBatteryRAM:
			return "MBC3_ExBatteryRAM";
		case CartridgeType::MBC3:
			return "MBC3";
		case CartridgeType::MBC3_ExRam:
			return "MBC3_ExRam";
		case CartridgeType::MBC4:
			return "MBC4";
		case CartridgeType::MBC4_ExRam:
			return "MBC4_ExRam";
		case CartridgeType::MBC4_ExBatteryRAM:
			return "MBC4_ExBatteryRAM";
		case CartridgeType::MBC5_ExBatteryRam:
			return "MBC5_ExBatteryRam";
		case CartridgeType::MBC5_Rumble_ExBatteryRam:
			return "MBC5_Rumble_ExBatteryRam";
		case CartridgeType::MBC5:
			return "MBC5";
		case CartridgeType::MBC5_ExRam:
			return "MBC5_ExRam";
		case CartridgeType::MBC5_Rumble:
			return "MBC5_Rumble";
		case CartridgeType::MBC5_Rumble_ExRam:
			return "MBC5_Rumble_ExRam";
		default:
			break;
	}

	return "Unknwon";
}