#include <cassert>

#include "Core/GPURegisters.h"
#include "Logging.h"

using namespace std;

////////////////////////////
/// LCD Control Register ///
////////////////////////////

// Initial value = 0x91
LCDControlRegister::LCDControlRegister()
	: Enabled(true)					// Bit 7
	, WindowTileMapSelect(0)		// Bit 6
	, WindowEnabled(false)			// Bit 5
	, BGWindowTileDataSelect(1)		// Bit 4
	, BGTileMapSelect(0)			// Bit 3
	, SpriteSize(0)					// Bit 2
	, SpriteEnabled(false)			// Bit 1
	, BGDisplay(true)				// Bit 0
	, RegisterByte(0x90)
{
}


// Where to retrieve BG tile numbers from
int LCDControlRegister::GetBGTileMapVRAMIndex() const
{
	if (BGTileMapSelect == 0)
		return 0x1800; // 9800h - 9BFFh
	else
		return 0x1C00; // 9C00h - 9FFFh
}

// Where to retrieve window tile numbers from
int LCDControlRegister::GetWindowTileMapVRAMIndex() const
{
	if (WindowTileMapSelect == 0)
		return 0x1800; // 9800h - 9BFFh
	else
		return 0x1C00; // 9C00h - 9FFFh
}

// Where to retrieve tile data from
int LCDControlRegister::GetTileDataVRAMIndex() const
{
	if (BGWindowTileDataSelect == 0)
		return 0x800; // 8800h - 97FFh
	else
		return 0x0; // 8000h - 8FFFh
}

void LCDControlRegister::WriteByte(uint8_t value)
{
	Enabled = (value & 0x80) == 0x80;
	WindowTileMapSelect = (value & 0x40) >> 6;
	WindowEnabled = (value & 0x20) == 0x20;
	BGWindowTileDataSelect = (value & 0x10) >> 4;
	BGTileMapSelect = (value & 0x08) >> 3;
	SpriteSize = (value & 0x04) >> 2;
	SpriteEnabled = (value & 0x02) == 0x02;
	BGDisplay = (value & 0x01) == 0x01;
	RegisterByte = value;
}

uint8_t LCDControlRegister::ReadByte() const
{
	return RegisterByte;
}


///////////////////////////
/// LCD Status Register ///
///////////////////////////

LCDStatusRegister::LCDStatusRegister() : LCDStatusRegister(true)
{
}

LCDStatusRegister::LCDStatusRegister(bool bCGB)
	: LYCLYCoincidenceIntEnabled(false) // Bit 6
	, OAMIntEnabled(false) // Bit 5
	, VBlankIntEnabled(false) // Bit 4
	, HBlankIntEnabled(false) // Bit 3
	, Mode(LCDMode::VBlank) // Bit 1-0
	, RegisterByte(0x1)
{
	// From observing BGB, it appears this is the only difference between CGB and non-CGB mode
	// Bit 4
	if (bCGB)
		LYCLYCoincidence = false;
	else
		LYCLYCoincidence = true;
}

void LCDStatusRegister::WriteByte(uint8_t value)
{
	LYCLYCoincidenceIntEnabled = (value & 0x40) == 0x40;
	OAMIntEnabled = (value & 0x20) == 0x20;
	VBlankIntEnabled = (value & 0x10) == 0x10;
	HBlankIntEnabled = (value & 0x08) == 0x08;
	// Lower 3 bits are readonly
	RegisterByte = value & 0xF8;
}

void LCDStatusRegister::Reset(bool bCGB)
{
	WriteByte(bCGB ? 0x01 : 0x05);
}

uint8_t LCDStatusRegister::ReadByte() const
{
	return RegisterByte | (LYCLYCoincidence << 2) | static_cast<uint8_t>(Mode);
}

///////////////////////////
///    LCD Positions    ///
///////////////////////////
LCDPositions::LCDPositions() 
: LCDPositions(true)
{
}

LCDPositions::LCDPositions(bool bCGB)
{
	Reset(bCGB);
}


void LCDPositions::Reset(bool bCGB)
{
	ScrollY = ScrollX = LineYCompare = WindowX = WindowY = WindowLineY = 0;
	LineY = bCGB ? 0x94 : 0x90;
}

//////////////////////////
/// Monochrome Palette ///
//////////////////////////

MonochromePalette::MonochromePalette()
	: BgPaletteRegisterByte(0)
{
	PalettePtrs[3] = const_cast<GemColour*>(&(GemPalette::Black()));
	PalettePtrs[2] = const_cast<GemColour*>(&(GemPalette::DarkGrey()));
	PalettePtrs[1] = const_cast<GemColour*>(&(GemPalette::LightGrey()));
	PalettePtrs[0] = const_cast<GemColour*>(&(GemPalette::White()));
}

void MonochromePalette::WriteBgPalette(uint8_t value)
{
	BGPalette[3] = (value & 0xC0) >> 6;
	BGPalette[2] = (value & 0x30) >> 4;
	BGPalette[1] = (value & 0x0C) >> 2;
	BGPalette[0] = (value & 0x03) >> 0;
	BgPaletteRegisterByte = value;
}

uint8_t MonochromePalette::ReadBgPalette()
{
	return BgPaletteRegisterByte;
}

const GemColour& MonochromePalette::GetColour(uint8_t colour_number)
{
	int index = BGPalette[colour_number];
	return *PalettePtrs[index];
}


//////////////////////
/// Colour Palette ///
//////////////////////

ColourPalette::ColourPalette()
{
	Reset();
}

ColourPalette::ColourPalette(string name)
	: Name(name)
{
	Reset();
}

void ColourPalette::Reset()
{
	PaletteIndex = 0;
	IndexAutoIncrement = false;
	PaletteIndexRegisterByte = 0;

	for (PaletteEntry& entry : Data)
	{
		entry.Byte0 = 0xFF;
		entry.Byte1 = 0x7F;
		entry.Colour.Red = 0x1F;
		entry.Colour.Green = 0x1F;
		entry.Colour.Blue = 0x1F;
	}
}

uint16_t ColourPalette::PaletteEntry::AsWord()
{
	return uint16_t((Byte1 << 8) | Byte0);
}

void ColourPalette::WritePaletteIndex(uint8_t value)
{
	PaletteIndex = int(value & 0x3F);
	IndexAutoIncrement = (value & 0x80) == 0x80;
	PaletteIndexRegisterByte = value;
}

uint8_t ColourPalette::ReadPaletteIndex()
{
	return PaletteIndexRegisterByte;
}

void ColourPalette::WritePaletteData(uint8_t value)
{
	// Each 2 bytes in the palette memory is arranged like so: XBBB,BBGG,GGGR,RRRR
	// This means green's value is contained in both the high and low byte.

	int index = PaletteIndex / 2;

	PaletteEntry& entry = Data[index];
	GemColour& colour = entry.Colour;

	if ((PaletteIndex & 1) == 0)
	{
		entry.Byte0 = value;
		colour.Red = value & 0x1F;
		colour.Green = (colour.Green & 0x18) | ((value & 0xE0) >> 5);
	}
	else
	{
		entry.Byte1 = value;
		colour.Blue = (value & 0x7C) >> 2;
		colour.Green = (colour.Green & 0x7) | ((value & 0x3) << 3);
	}

	if (IndexAutoIncrement)
	{
		PaletteIndex = (PaletteIndex + 1) % 64;
	}
}

uint8_t ColourPalette::ReadPaletteData()
{
	int index = PaletteIndex / 2;
	PaletteEntry& entry = Data[index];

	if ((PaletteIndex & 1) == 0)
		return entry.Byte0;
	else
		return entry.Byte1;
}

const GemColour& ColourPalette::GetColour(int palette, int colour_number)
{
	int index = (palette * 4) + colour_number; // Every 4 colours is 1 palette
	return Data[index].Colour;
}

//////////////////////////
/// CGB Tile Attribute ///
//////////////////////////

CGBTileAttribute::CGBTileAttribute()
{
}

CGBTileAttribute::CGBTileAttribute(uint8_t value)
{
	DecodeFromByte(value);
}

void CGBTileAttribute::DecodeFromByte(uint8_t value)
{
	PriorityOverSprites = (value & 0x80) == 0x80;
	VerticalFlip = (value & 0x40) == 0x40;
	HorizontalFlip = (value & 0x20) == 0x20;
	VRAMBank = (value & 0x08) == 0x08 ? 1 : 0;
	Palette = value & 0x07;
}


//////////////////////////////
/// Sprite Attribute Entry ///
//////////////////////////////

SpriteData::SpriteData()
{
	Reset(true);
}

void SpriteData::Reset(bool bCGB)
{
	this->bCGB = bCGB;
	YPos = 0;
	XPos = 0;
	Tile = 0;
	BehindBG = false;
	VerticalFlip = false;
	HorizontalFlip = false;
	DMGPalette = -1;
	VRAMBank = -1;
	CGBPalette = -1;
	IsZero = true;
}

void SpriteData::DecodeFromOAM(uint16_t addr, uint8_t* oam)
{
	assert(oam != nullptr);
	DecodeFromOAM(addr, oam[0], oam[1], oam[2], oam[3]);
}

void SpriteData::DecodeFromOAM(uint16_t addr, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3)
{
	Address = addr;

	// The gameboy's memory actually stores X+8 and Y+16, this is so that each byte can be treated as an unsigned number (since a signed byte's
	// range [-127,128] wouldn't work) and still allow positions outside the BG area. 
	// We want to decode the actual (signed) positions, so we cast to an int16.
	YPos = byte0 - 16;
	XPos = byte1 - 8;
	Tile = byte2;

	uint8_t attrs = byte3;

	BehindBG =			(attrs & 0x80) == 0x80;
	VerticalFlip =		(attrs & 0x40) == 0x40;
	HorizontalFlip =	(attrs & 0x20) == 0x20;
	DMGPalette =		(attrs & 0x10) >> 4;
	
	// VRAMBank and CGBPalette are only valid in CGB mode but that's
	// enforced by the GPU class when it selects which to use for a sprite
	VRAMBank = (attrs & 0x08) == 0 ? 0 : 1;
	CGBPalette = attrs & 0x07;

	IsZero = byte0 == 0 && byte1 == 0 && byte2 == 0 && byte3 == 0;
}

//////////////////////////////
/// VRAM DMA Transfer Reg. ///
//////////////////////////////

DMATransferRegisters::DMATransferRegisters()
{
	Reset();
}

void DMATransferRegisters::Reset()
{
	Source = 0;
	Dest = 0;
	HBlankMode = false;
	LengthCode = 0x7F;
	Active = false;
}

uint8_t DMATransferRegisters::ReadByte(uint16_t addr) const
{
	switch (addr)
	{
		case 0xFF51:
			return (Source & 0xFF00) >> 8;
		case 0xFF52:
			return Source & 0xFF;
		case 0xFF53:
			return (Dest & 0xFF00) >> 8;
		case 0xFF54:
			return Dest & 0xFF;
		case 0xFF55:
			return uint8_t(!Active << 7) | LengthCode;
		default:
			throw exception("Invalid register address");
	}
}

void DMATransferRegisters::WriteByte(uint16_t addr, uint8_t value)
{
	switch (addr)
	{
		case 0xFF51:
			Source = (value << 8) | (Source & 0xFF);
			break;
		case 0xFF52:
			Source = (Source & 0xFF00) | (value & 0xF0);
			break;
		case 0xFF53:
			// Ignore upper 3 bits
			Dest = ((value & 0x1F) << 8) | (Dest & 0xFF);
			break;
		case 0xFF54:
			Dest = (Dest & 0xFF00) | (value & 0xF0);
			break;
		case 0xFF55:
			if (!Active)
			{
				// LengthCode = (Length / 16) - 1
				LengthCode = value & 0x7F;
				Length = (LengthCode + 1) << 4;
				HBlankMode = (value & 0x80) != 0;
				Active = true;
			}
			else
			{
				if ((value & 0x80) == 0)
				{
					LengthCode = 0x7F;
					Length = 0;
					HBlankMode = false;
					Active = false;
				}
				else
				{
					LOG_WARN("Unexpected write (value: %d) to DMA register FF55; DMA is already active", value);
				}
			}
			break;
		default:
			throw exception("Invalid register address");
	}
}
