#pragma once

#include <string>
#include <cstdint>

#include "Colour.h"

enum class LCDMode
{
	HBlank = 0x0,
	VBlank = 0x1, // 
	ReadingOAM = 0x2, // LCD controller reading from OAM
	ReadingVRAM = 0x3, // LCD controller reading from OAM and VRAM and transferring to LCD driver
};

// 0xFF40
struct LCDControlRegister
{	
	LCDControlRegister();
	int GetTileDataVRAMIndex() const;
	int GetBGTileMapVRAMIndex() const;
	int GetWindowTileMapVRAMIndex() const;
	void WriteByte(uint8_t value);
	uint8_t ReadByte() const;

	bool Enabled;				// Bit 7
	int WindowTileMapSelect;	// Bit 6
	bool WindowEnabled;			// Bit 5
	int BGWindowTileDataSelect;	// Bit 4
	int BGTileMapSelect;		// Bit 3
	int	SpriteSize;				// Bit 2
	bool SpriteEnabled;			// Bit 1
	bool BGDisplay;				// Bit 0
	uint8_t RegisterByte;
};

// 0xFF41 (7bit register)
struct LCDStatusRegister
{
	// Initial state = 0x01
	bool LYCLYCoincidenceIntEnabled; // Bit 6
	bool OAMIntEnabled; // Bit 5
	bool VBlankIntEnabled; // Bit 4
	bool HBlankIntEnabled; // Bit 3
	bool LYCLYCoincidence; // Bit 2
	LCDMode Mode; // Bit 1-0
	uint8_t RegisterByte;

	LCDStatusRegister();
	LCDStatusRegister(bool bCGB);
	void WriteByte(uint8_t value);
	void Reset(bool bCGB);
	uint8_t ReadByte() const;
};

struct LCDPositions
{
	LCDPositions();
	LCDPositions(bool bCGB);
	uint8_t ScrollY; // 0xFF42
	uint8_t ScrollX; // 0xFF43
	uint8_t LineY; // 0xFF44: Ranges from 0-153. 144-153 occurs during VBlank period. Equiv to LY but I don't like that name.
	uint8_t LineYCompare; // 0xFF45
	uint8_t WindowY; // 0xFF4A
	uint8_t WindowX; // 0xFF4B
	uint8_t WindowLineY; // This isn't a register, but we need a counter for the current line of the window being rendered.

	void Reset(bool bCGB);
};

struct MonochromePalette
{
	MonochromePalette();
	void WriteBgPalette(uint8_t value);
	uint8_t ReadBgPalette();
	const GemColour& GetColour(uint8_t colour_number);

	GemColour* PalettePtrs[4];
	uint8_t BGPalette[4];
	uint8_t BgPaletteRegisterByte;

	uint8_t SpritePalette0 = 0;
	uint8_t SpritePalette1 = 0;
};

struct ColourPalette
{
	struct PaletteEntry
	{
		GemColour Colour;
		uint8_t Byte0;
		uint8_t Byte1;
		uint16_t AsWord();
	};

	ColourPalette();
	ColourPalette(std::string name);

	void WritePaletteIndex(uint8_t value);
	uint8_t ReadPaletteIndex();
	void WritePaletteData(uint8_t value);
	uint8_t ReadPaletteData();
	const GemColour& GetColour(int palette, int colour_number);

	void Reset();

	ColourPalette& operator=(const ColourPalette& other);

	char Name[16];

	// Addresses 1 of 64 bytes inside the palette data memory. Each 2 bytes define 1 colour for a total of 32 colours. 
	uint8_t PaletteIndex = 0;
	bool IndexAutoIncrement = false;
	uint8_t PaletteIndexRegisterByte;

	PaletteEntry Data[32]; // Every 4 entries is 1 palette
};

struct CGBTileAttribute
{
	uint8_t Palette              = 0;
	uint8_t VRAMBank             = 0;
	bool HorizontalFlip      = false;
	bool VerticalFlip        = false;
	bool PriorityOverSprites = false;

	CGBTileAttribute();
	CGBTileAttribute(uint8_t value);
	void DecodeFromByte(uint8_t value);
};

struct SpriteData
{
	short YPos; // Byte 0
	short XPos; // Byte 1
	uint8_t Tile; // Byte 2
	bool BehindBG; // Bit 7
	bool VerticalFlip; // Bit6
	bool HorizontalFlip; // Bit5
	int DMGPalette; // Bit4
	int VRAMBank; // Bit3
	int CGBPalette; // Bit2-0

	bool bCGB;
	bool IsZero;
	uint16_t Address;

	SpriteData();
	void Reset(bool bCGB);
	void DecodeFromOAM(uint16_t addr, uint8_t* oam);
	void DecodeFromOAM(uint16_t addr, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3);
};

struct DMATransferRegisters
{
	DMATransferRegisters();

	uint8_t ReadByte(uint16_t addr) const;
	void WriteByte(uint16_t addr, uint8_t value); // Returns true if a transfer should be initiated
	void Reset();

	uint16_t Source;
	uint16_t Dest;
	short Length;
	uint8_t LengthCode;
	bool HBlankMode; // Transfer 16 bytes at a time during each hblank period
	bool Active; // Visible on read
};