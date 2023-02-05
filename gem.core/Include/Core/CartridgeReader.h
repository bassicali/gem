#pragma once

#include <cstdint>

enum class CartridgeType
{
	RomOnly = 0x00,						// No external RAM or ROM banks
	MBC1 = 0x01,						// External ROM banks
	MBC1_ExRAM = 0x02,					// External ROM and RAM banks
	MBC1_ExBatteryRAM = 0x03,			// External ROM banks and battery powered RAM banks
	MBC2 = 0x05,
	MBC2_Battery = 0x06,
	ROM_ExRam = 0x08,
	ROM_ExBatteryRAM = 0x09,
	MMM01 = 0x0B,
	MMM01_ExRam = 0x0C,
	MMM01_ExBatteryRAM = 0x0D,
	MBC3_Timer_Battery = 0x0F,
	MBC3_Timer_ExBatteryRAM = 0x10,
	MBC3 = 0x11,
	MBC3_ExRam = 0x12,
	MBC3_ExBatteryRAM = 0x13,
	MBC4 = 0x15,
	MBC4_ExRam = 0x16,
	MBC4_ExBatteryRAM = 0x17,
	MBC5 = 0x19,
	MBC5_ExRam = 0x1A,
	MBC5_ExBatteryRam = 0x1B, 
	MBC5_Rumble = 0x1C,
	MBC5_Rumble_ExRam = 0x1D,
	MBC5_Rumble_ExBatteryRam = 0x1E,
};

enum class MBCVersion
{
	MBC1,
	MBC2,
	MBC3,
	MBC4,
	MBC5
};

enum class CGBSupport
{
	BackwardsCompatible = 0,
	CGBOnly = 1,
	NoCGBSupport = 2,
};

struct CartridgeProperties
{
	CartridgeProperties()
		: Type(CartridgeType::RomOnly)
		, Version(MBCVersion::MBC1)
		, CGBCompatability(CGBSupport::NoCGBSupport)
		, HasExtRam(false)
		, ExtRamHasBattery(false)
		, HasRTC(false)
		, NumRAMBanks(0)
		, ROMSize(0)
		, RAMSize(0)
		, RAMBankSize(0)
	{
		memset(Title, 0, 16 * sizeof(char));
	}

	CartridgeProperties& operator=(const CartridgeProperties& other);

	CartridgeType Type;
	MBCVersion Version;
	CGBSupport CGBCompatability;
	bool HasExtRam;
	bool ExtRamHasBattery;
	bool HasRTC;

	char Title[16];
	int NumROMBanks;
	int ROMSize;
	int NumRAMBanks;
	int RAMSize;
	int RAMBankSize;

};

class CartridgeReader
{
	public:
		CartridgeReader();
		~CartridgeReader();
		uint8_t ReadByte();
		uint8_t ReadByte(int index) const;
		unsigned int GetSize() const { return size; }
		uint8_t* GetRomDataPointer() const { return romData; }

		void LoadFile(const char* file);
		bool IsLoaded() const { return isLoaded; }

		// Header data
		const CartridgeProperties& Properties() const { return cartProps; }
		CGBSupport GetCGBCompatability() const { return cartProps.CGBCompatability; }
		
		const std::string& SaveGameFile() const { return gemSavePath; }
		const bool SaveGameFileExists() const { return gemSaveExists; }

		static std::string ROMTypeString(CartridgeType type);
		static std::string CGBSupportString(CGBSupport type);
		static void DecodeHeader(const uint8_t header_data[], CartridgeProperties& props);

	private:
		bool isLoaded;

		CartridgeProperties cartProps;

		uint8_t* romData;
		unsigned int size;
		unsigned int cursor;

		std::string gemSavePath;
		bool gemSaveExists;
};