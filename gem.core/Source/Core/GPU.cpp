
#include <cstdio>
#include <cassert>

#include "Logging.h"

#include "Core/GPU.h"

using namespace std;

GPU::GPU()
	: vram(VRAMSize, true)
	, oam(OAMSize, true)
	, bgColourPalette(string("BG Palette"))
	, sprColourPalette(string("Sprite Palette"))
	, frameBuffer(LCDWidth, LCDHeight)
	, correctionMode(CorrectionMode::Washout)
	, brightness(1.0f)
	, bCGB(false)
	, tAcc(0)
	, dmaDest(0)
	, dmaSrc(0)
	, vramBank(0)
	, vramOffset(0)
{
}

void GPU::Reset(bool bCGB)
{
	this->bCGB = bCGB;

	vram.Fill(0);
	oam.Fill(0);

	for (int i = 0; i < NumSprites; i++)
		sprites[i].Reset(bCGB);

	positions.Reset(bCGB);
	stat.Reset(bCGB);

	bgColourPalette.Reset();
	sprColourPalette.Reset();

	tAcc = 0;
}

void GPU::SetCartridge(std::shared_ptr<CartridgeReader> ptr)
{
	cart = ptr;
}

void GPU::SetInterruptController(std::shared_ptr<InterruptController> ptr)
{
	interrupts = ptr;
}

void GPU::SetMMU(std::shared_ptr<IMappedComponent> ptr)
{
	mmu = ptr;
}

void GPU::WriteVRAMBank(uint8_t value)
{
	if (cart->GetCGBCompatability() == CGBSupport::NoCGBSupport)
	{
		LOG_WARN("VRAM banking is only allowed for CGB compatible games.");
	}

	vramBank = value & 0x1;
	vramOffset = vramBank * 0x2000;
}

void GPU::TickStateMachine(int t_cycles)
{
	assert(t_cycles >= 0);

	if (!control.Enabled)
		return;

	LCDMode sin = stat.Mode;
	LCDMode sout = sin;

	tAcc += t_cycles;

	// Pan docs indicates that the duration of each state is variable. All emulators I've seen appear to use the midpoint in the ranges that
	// pan docs gives. E.g. (0) lasts 201-207 T cycles, and emulators just use 204.
	switch (sout)
	{
		case LCDMode::HBlank: // (0)
		{
			if (tAcc >= 204)
			{
				tAcc -= 204;
				IncLineY();

				// Reached the last line
				if (positions.LineY == LCDHeight)
				{
					sout = LCDMode::VBlank;
					interrupts->VBlankRequested = true;

					if (stat.VBlankIntEnabled)
						interrupts->LCDStatusRequested = true;
				}
				else
				{
					sout = LCDMode::ReadingOam;

					if (stat.OamIntEnabled)
						interrupts->LCDStatusRequested = true;
				}
			}
			break;
		}

		case LCDMode::VBlank: // (1)
		{
			// LineY will increment 10 times while in VBlank (every 114 M cycles). And each time LycLyCompare() must be called
			if (tAcc >= 456)
			{
				tAcc -= 456;
				IncLineY();

				// Seems like vblank gets to last for an extra line (line 0) but when switching to ReadingOam LY is reset to 0 again
				if (positions.LineY == 153)
				{
					positions.LineY = 0;
					positions.WindowLineY = 0;
					LycLyCompare();
				}
				else if (positions.LineY == 1)
				{
					// We entered this state when LineY was 144. This is 11 lines later
					sout = LCDMode::ReadingOam;
					positions.LineY = 0;

					if (stat.OamIntEnabled)
						interrupts->LCDStatusRequested = true;
				}
			}
			break;
		}

		case LCDMode::ReadingOam: // (2)
		{
			if (tAcc >= 80)
			{
				tAcc -= 80;
				sout = LCDMode::ReadingVRam;
			}

			break;
		}

		case LCDMode::ReadingVRam: // (3)
		{
			if (tAcc >= 172)
			{
				tAcc -= 172;
				RenderLine();

				sout = LCDMode::HBlank;

				if (stat.HBlankIntEnabled)
					interrupts->LCDStatusRequested = true;

				if (dma.Active)
				{
					if (dma.Length <= 0)
					{
						dma.Reset();
						dmaSrc = 0;
						dmaDest = 0;
					}
					else
					{
						for (int i = 0; i < 16; i++)
							vram[dmaDest++] = mmu->ReadByte(dmaSrc++);

						dma.Length -= 16;
					}
				}
			}
			break;
		}
	}

	stat.Mode = sout;
}

void GPU::IncLineY()
{
	positions.LineY++; 
	LycLyCompare();
}

void GPU::LycLyCompare()
{
	if (positions.LineY == positions.LineYCompare)
	{
		stat.LycLyCoincidence = true;

		if (stat.LycLyCoincidenceIntEnabled)
			interrupts->LCDStatusRequested = true;
	}
	else
	{
		stat.LycLyCoincidence = false;
	}
}

void GPU::SetLCDControl(uint8_t value)
{
	LCDControlRegister old = control;
	control.WriteByte(value);

	// When the LCD gets disabled, some values have to be reset
	if (old.Enabled == true && control.Enabled == false)
	{
		stat.Mode = LCDMode::HBlank;
		positions.LineY = 0;
		positions.WindowLineY = 0;
		tAcc = 0;
		interrupts->LCDStatusRequested = false;
	}

	if (old.WindowEnabled == false && control.WindowEnabled == true)
	{
		// TODO: noticed this behaviour in Gearboy emulator
		// https://github.com/drhelius/Gearboy
		if (positions.WindowLineY == 0 && 
			positions.LineY > positions.WindowY && 
			positions.LineY < LCDHeight)
			positions.WindowY = 144;
	}
}

void GPU::RenderLine()
{
	RenderBGLine();
	RenderWindowLine();
	RenderSpriteLine();
}

void GPU::GetTilePixelRow(int line_pos, TilePixelRow& pixels, CgbTileAttribute& tile_attr)
{
	// This function takes a position in [0,255] and combined with LineY+SCY 
	// computes the colours numbers for the 8 pixel row that needs to be rendedered
	// In CGB mode it also fetches the tile attributes

	const int abs_ln = (positions.LineY + positions.ScrollY) % TileMapWidth;
	const int map_row_start = (abs_ln / 8) * 32;
	int abs_col = line_pos + positions.ScrollX;
	int map_index = control.GetBgTileMapVRamIndex() + map_row_start + (abs_col / 8) % 32; // Addr of 1byte tile num

	uint8_t tile_num;

	if (control.BgWindowTileDataSelect == 0)
		tile_num = int8_t(vram[map_index]) + 128;
	else
		tile_num = vram[map_index];

	if (bCGB)
		tile_attr.DecodeFromByte(vram[0x2000 + map_index]);

	int pixel_row;
	if (bCGB && tile_attr.VerticalFlip)
		pixel_row = 7 - (abs_ln % 8);
	else
		pixel_row = abs_ln % 8;

	int tile_data_index = control.GetTileDataVRamIndex() + (tile_num * 16) + (pixel_row * 2);

	bool bank_1 = bCGB && tile_attr.VRamBank == 1;
	bool horizontal_flip = bCGB && tile_attr.HorizontalFlip;
	ReadPixels(pixels, tile_data_index, bank_1, horizontal_flip);
}

void GPU::RenderBGLine()
{
	static TilePixelRow pixels;
	static CgbTileAttribute tile_attr;

	// Skip if BG is disabled
	if (!control.BgDisplay && !bCGB)
		return;

	for (uint8_t i = 0; i < LCDWidth;)
	{
		GetTilePixelRow(i, pixels, tile_attr);

		int buff_index = positions.LineY * frameBuffer.Width + i;
		assert(buff_index < frameBuffer.Count());

		int skip = (i + positions.ScrollX) % 8;

		if (bCGB)
		{
			uint8_t palette_index = tile_attr.Palette;

			for (int p = skip, j = 0;
					p < 8 && i < LCDWidth; 
					p++, j++)
			{
				GemColour& px = frameBuffer[buff_index + j];
				px = bgColourPalette.GetColour(palette_index, pixels[p]);
				px.Correct(correctionMode, brightness);
				px.ColourNumber = pixels[p];
				px.Priority = tile_attr.PriorityOverSprites;
				i++;
			}
		}
		else
		{
			for (int p = skip, j = 0; 
					p < 8 && i < LCDWidth; 
					p++, j++)
			{
				frameBuffer[buff_index + j] = bgMonoPalette.GetColour(pixels[p]);
				frameBuffer[buff_index + j].ColourNumber = pixels[p];
				i++;
			}
		}
	}
}

void GPU::GetWindowTilePixelRow(int line_pos, TilePixelRow& pixels, CgbTileAttribute& tile_attr)
{
	// This function takes a position in [0,255] and combined with LineY+SCY 
	// computes the colour numbers for the 8px-wide row that needs to be rendered.
	// In CGB mode it also fetches the tile attributes

	const int abs_ln = positions.WindowLineY;
	const int map_row_start = (positions.WindowLineY / 8) * 32;

	int map_index = control.GetWindowTileMapVRamIndex() + map_row_start + (line_pos / 8); // Addr of 1byte tile num

	uint8_t tile_num;

	if (control.BgWindowTileDataSelect == 0)
		tile_num = int8_t(vram[map_index]) + 128;
	else
		tile_num = vram[map_index];

	if (bCGB)
		tile_attr.DecodeFromByte(vram[0x2000 + map_index]);

	int pixel_row;
	if (bCGB && tile_attr.VerticalFlip)
		pixel_row = 7 - (abs_ln % 8);
	else
		pixel_row = abs_ln % 8;

	int tile_data_index = control.GetTileDataVRamIndex() + (tile_num * 16) + (pixel_row * 2);

	bool bank_1 = bCGB && tile_attr.VRamBank == 1;
	bool horizontal_flip = bCGB && tile_attr.HorizontalFlip;
	ReadPixels(pixels, tile_data_index, bank_1, horizontal_flip);
}

void GPU::RenderWindowLine()
{
	static CgbTileAttribute tile_attr;
	static TilePixelRow pixels;

	// Skip if window is disabled
	if (!control.WindowEnabled || (!bCGB && !control.BgDisplay))
		return;

	if (positions.WindowX > 166 || positions.WindowY > positions.LineY)
		return;

	int xpos = 7 - positions.WindowX;
	for (int i = 0; i < LCDWidth;)
	{
		if (i < positions.WindowX - 7)
		{
			i++;
			xpos++;
			continue;
		}

		GetWindowTilePixelRow(xpos, pixels, tile_attr);

		int buff_index = positions.LineY * frameBuffer.Width + i;
		assert(buff_index < frameBuffer.Count());
		
		int skip = xpos % 8;

		if (bCGB)
		{
			uint8_t palette_index = tile_attr.Palette;

			for (int p = skip, j = 0;
					p < 8 && i < LCDWidth;
					p++, j++)
			{
				GemColour& px = frameBuffer[buff_index + j];
				px = bgColourPalette.GetColour(palette_index, pixels[p]);
				px.Correct(correctionMode, brightness);
				px.ColourNumber = pixels[p];
				i++;
				xpos++;
			}
		}
		else
		{
			for (int p = skip, j = 0;
					p < 8 && i < LCDWidth;
					p++, j++)
			{
				frameBuffer[buff_index + j] = bgMonoPalette.GetColour(pixels[p]);
				frameBuffer[buff_index + j].ColourNumber = pixels[p];
				i++;
				xpos++;
			}
		}
	}

	positions.WindowLineY++;
}

void GPU::RenderSpriteLine()
{
	// Skip if sprites are disabled
	if (!control.SpriteEnabled)
		return;

	int sprite_height = control.SpriteSize == 0 
						? 8 : 16;

	static TilePixelRow pixels;
	int sprites_rendered = 0;

	for (int i = 0; i < NumSprites; i++)
	{
		// Lower number sprites have higher priority (they're drawn overtop of lower numbers)
		// We can account for this by rendering the high priority ones last.
		int sprite_num = NumSprites - i - 1;

		// Since we've been actively decoding sprite attributes on each write to the OAM,
		// we don't have to worry about doing it in this hot loop and function.
		// TODO: the same can be done with tiles where on each vram write, the corresponding tile is deserialized this loop simply copies the line of pixels that it needs
		SpriteData& sprite = sprites[sprite_num];

		// Only render if LY occurs between the top and bottom of this sprite
		if (positions.LineY >= sprite.YPos && positions.LineY < (sprite.YPos + sprite_height)
			&& sprite.XPos >= -7 && sprite.XPos <= LCDWidth)
		{
			// Only 10 sprites can be drawn per line
			if (++sprites_rendered > 10)
				return;

			int abs_ln = positions.LineY - sprite.YPos;

			int pixel_row = 0;
			if (sprite.VerticalFlip)
				pixel_row = (sprite_height - 1) - (abs_ln % sprite_height);
			else
				pixel_row = abs_ln % sprite_height;

			// For sprites that are 16px tall, any pixels in the lower half of the sprite
			// will be located 16 bytes after the top half's tile. In other words, the top and 
			// bottom are stored as back-to-back tiles
			int tile = sprite.Tile;
			if (sprite_height == 16)
			{
				if (pixel_row >= 8)
				{
					tile |= 0x01;
					pixel_row -= 8;
				}
				else
				{
					tile &= 0xFE;
				}
			}

			// It seems sprite tile data is always at located in 8000h-8FFFh, the offset into VRAM is 0
			int data_index = (tile * 16) + (pixel_row * 2);

			uint8_t b0;
			uint8_t b1;
			if (bCGB && sprite.VRamBank == 1)
			{
				b0 = vram[0x2000 + data_index];
				b1 = vram[0x2000 + data_index + 1];
			}
			else
			{
				b0 = vram[data_index];
				b1 = vram[data_index + 1];
			}

			// Early out for colour number 0 which is always transparent
			if (b0 == 0 && b1 == 0)
				continue;

			DecodePixels(pixels, b0, b1, sprite.HorizontalFlip);

			int buff_index = positions.LineY * frameBuffer.Width + sprite.XPos;

			if (bCGB)
			{
				for (int i = 0; i < 8; i++)
				{
					if (buff_index + i >= frameBuffer.Count())
						break;

					int xpos = sprite.XPos + i;
					if (xpos >= 0 && xpos <= LCDWidth)
					{
						GemColour& px = frameBuffer[buff_index + i];

						bool replaced = px.ReplaceWithSpritePixel(sprColourPalette.GetColour(sprite.CGBPalette, pixels[i]),
																	pixels[i], 
																	sprite.BehindBg, 
																	bCGB && control.BgDisplay);

						if (replaced && pixels[i] != 0) // Don't scale colour 0
						{
							px.Correct(correctionMode, brightness);
						}
					}
				}
			}
			else
			{
				for (int i = 0; i < 8; i++)
				{
					if (buff_index + i > frameBuffer.Count())
						break;

					int xpos = sprite.XPos + i;
					if (xpos >= 0 && xpos <= LCDWidth)
					{
						frameBuffer[buff_index + i].ReplaceWithSpritePixel(sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[i]), pixels[i], sprite.BehindBg, false);
					}
				}
			}
		}
	}
}

inline void GPU::ReadPixels(TilePixelRow& pixels, int vram_index, bool read_bank_1, bool horizontal_flip)
{
	uint8_t b0;
	uint8_t b1;
	if (bCGB && read_bank_1)
	{
		b0 = vram[0x2000 + vram_index];
		b1 = vram[0x2000 + vram_index + 1];
	}
	else
	{
		b0 = vram[vram_index];
		b1 = vram[vram_index + 1];
	}

	DecodePixels(pixels, b0, b1, horizontal_flip);
}

inline void GPU::DecodePixels(TilePixelRow& pixels, uint8_t b0, uint8_t b1, bool horizontal_flip) 
{
	// b0 and b1 have the 8 pixels that we need to render
	// b1 has the higher of the two bits for a pixel and b0 has the lower bit.
	if (horizontal_flip)
	{
		pixels.p7 = (b1 & 0x80) >> 6 | (b0 & 0x80) >> 7;
		pixels.p6 = (b1 & 0x40) >> 5 | (b0 & 0x40) >> 6;
		pixels.p5 = (b1 & 0x20) >> 4 | (b0 & 0x20) >> 5;
		pixels.p4 = (b1 & 0x10) >> 3 | (b0 & 0x10) >> 4;
		pixels.p3 = (b1 & 0x08) >> 2 | (b0 & 0x08) >> 3;
		pixels.p2 = (b1 & 0x04) >> 1 | (b0 & 0x04) >> 2;
		pixels.p1 = (b1 & 0x02) >> 0 | (b0 & 0x02) >> 1;
		pixels.p0 = (b1 & 0x01) << 1 | (b0 & 0x01) >> 0;
	}
	else
	{
		pixels.p0 = (b1 & 0x80) >> 6 | (b0 & 0x80) >> 7;
		pixels.p1 = (b1 & 0x40) >> 5 | (b0 & 0x40) >> 6;
		pixels.p2 = (b1 & 0x20) >> 4 | (b0 & 0x20) >> 5;
		pixels.p3 = (b1 & 0x10) >> 3 | (b0 & 0x10) >> 4;
		pixels.p4 = (b1 & 0x08) >> 2 | (b0 & 0x08) >> 3;
		pixels.p5 = (b1 & 0x04) >> 1 | (b0 & 0x04) >> 2;
		pixels.p6 = (b1 & 0x02) >> 0 | (b0 & 0x02) >> 1;
		pixels.p7 = (b1 & 0x01) << 1 | (b0 & 0x01) >> 0;
	}
}

void GPU::WriteRegister(uint16_t addr, uint8_t value)
{
	switch (addr)
	{
		case 0xFF40:
			SetLCDControl(value);
			break;
		case 0xFF41:
			GetLCDStatus().WriteByte(value);
			break;
		case 0xFF42:
			positions.ScrollY = value;
			break;
		case 0xFF43:
			positions.ScrollX = value;
			break;
		case 0xFF44:
			// Writes will reset this register
			positions.LineY = 0;
			positions.WindowLineY = 0;
			break;
		case 0xFF45:
			positions.LineYCompare = value;
			break;
		case 0xFF46:
			DmaTransferToOam(value);
			break;
		case 0xFF47:
			bgMonoPalette.WriteBgPalette(value);
			break;
		case 0xFF48:
			sprMonoPalettes[0].WriteBgPalette(value);
			break;
		case 0xFF49:
			sprMonoPalettes[1].WriteBgPalette(value);
			break;
		case 0xFF4A:
			positions.WindowY = value;
			break;
		case 0xFF4B:
			positions.WindowX = value;
			break;
		case 0xFF4F:
			WriteVRAMBank(value);
			break;
		case 0xFF51:
		case 0xFF52:
		case 0xFF53:
		case 0xFF54:
		case 0xFF55:
		{
			bool prev_active = dma.Active;

			dma.WriteByte(addr, value);

			if (dma.Active && !prev_active)
			{
				uint16_t offset = vramBank * 0x2000;
				dmaSrc = dma.Source;
				dmaDest = dma.Dest + offset;

				// Peform a general purpose DMA right now
				if (!dma.HBlankMode)
				{
					for (int i = 0; i < dma.Length; i++)
						vram[dmaDest++] = mmu->ReadByte(dmaSrc++);

					dmaSrc = 0;
					dmaDest = 0;
					dma.Reset();
				}
			}

			break;
		}
		case 0xFF68:
			bgColourPalette.WritePaletteIndex(value);
			break;
		case 0xFF69:
			bgColourPalette.WritePaletteData(value);
			break;
		case 0xFF6A:
			sprColourPalette.WritePaletteIndex(value);
			break;
		case 0xFF6B:
			sprColourPalette.WritePaletteData(value);
			break;
		default:
			LOG_VERBOSE("[GPU] WriteRegister: Illegal or unsupported address: %Xh", addr);
			break;
	}
}

uint8_t GPU::ReadRegister(uint16_t addr)
{
	switch (addr)
	{
		case 0xFF40:
			return GetLCDControl().ReadByte();
		case 0xFF41:
			return GetLCDStatus().ReadByte();
		case 0xFF42:
			return positions.ScrollY;
		case 0xFF43:
			return positions.ScrollX;
		case 0xFF44:
			return positions.LineY;
		case 0xFF45:
			return positions.LineYCompare;
		case 0xFF47:
			return bgMonoPalette.ReadBgPalette();
		case 0xFF48:
			return sprMonoPalettes[0].ReadBgPalette();
		case 0xFF49:
			return sprMonoPalettes[1].ReadBgPalette();
		case 0xFF4A:
			return positions.WindowY;
		case 0xFF4B:
			return positions.WindowX;
		case 0xFF4F:
			return ReadVRAMBank();
		case 0xFF51:
		case 0xFF52:
		case 0xFF53:
		case 0xFF54:
		case 0xFF55:
			return dma.ReadByte(addr);
		case 0xFF68:
			return bgColourPalette.ReadPaletteIndex();
		case 0xFF69:
			return bgColourPalette.ReadPaletteData();
		case 0xFF6A:
			return sprColourPalette.ReadPaletteIndex();
		case 0xFF6B:
			return sprColourPalette.ReadPaletteData();
		default:
			LOG_VERBOSE("[GPU] ReadRegister: Illegal or unsupported address: %Xh", addr);
			break;
	}

	return 0;
}

uint8_t GPU::ReadByteVRAM(uint16_t addr)
{
	if (stat.Mode == LCDMode::ReadingVRam)
	{
		LOG_VIOLATION("[GPU] VRAM access during ReadingVRam mode (3) (or BetweenLines(5))");
	}

	int relative_addr = vramOffset + (addr & 0x1FFF);
	return vram[relative_addr];
}

void GPU::WriteByteVRAM(uint16_t addr, uint8_t value)
{
	if (stat.Mode == LCDMode::ReadingVRam)
		LOG_VIOLATION("Cannot access VRAM during ReadingVRam(3) mode");

	int relative_addr = vramOffset + (addr & 0x1FFF);
	vram[relative_addr] = value;
}

uint8_t GPU::ReadByteOAM(uint16_t addr)
{
	if (stat.Mode == LCDMode::ReadingOam)
		LOG_VIOLATION("[GPU] OAM access during BetweenBlanks(5) mode");

	return oam[addr & 0xFF];
}

void GPU::DmaTransferToOam(uint8_t source)
{
	// NOTE: A DMA transfer takes 160us to complete during which time only high ram can be read.
	//       A game written for a game boy will/should account for this in its logic. So as with the
	//       the rest of the emulator we won't attempt to stop it

	// The value written to FF46h represents the source address divided by 100h
	int actual_source = source << 8;

	for (uint8_t i = 0; i < GPU::OAMSize; i += 4)
	{
		uint8_t byte0 = mmu->ReadByte(actual_source + i + 0);
		uint8_t byte1 = mmu->ReadByte(actual_source + i + 1);
		uint8_t byte2 = mmu->ReadByte(actual_source + i + 2);
		uint8_t byte3 = mmu->ReadByte(actual_source + i + 3);
		WriteDWordOAM(0xFE00 | i, byte0, byte1, byte2, byte3);
	}
}

void GPU::WriteDWordOAM(uint16_t addr, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3)
{
	if (stat.Mode == LCDMode::ReadingOam)
		LOG_VIOLATION("[GPU] OAM access during BetweenBlanks(5) mode");

	int offset = addr & 0xFF;
	if (offset > 0x9F)
		throw new exception("OAM index out of range");

	oam[offset + 0] = byte0;
	oam[offset + 1] = byte1;
	oam[offset + 2] = byte2;
	oam[offset + 3] = byte3;

	// Decode the newly updated sprite entry right away
	int sprite_index = offset / 4;
	uint16_t base_addr = addr & 0xFFFC;
	sprites[sprite_index].DecodeFromOAM(base_addr, byte0, byte1, byte2, byte3);
}

void GPU::WriteByteOAM(uint16_t addr, uint8_t value)
{
	if (stat.Mode == LCDMode::ReadingOam)
		LOG_VIOLATION("[GPU] OAM access during BetweenBlanks(5) mode");

	int offset = addr & 0xFF;
	if (offset > 0x9F)
		throw new exception("OAM index out of range");

	oam[offset] = value;
	
	// Decode the newly updated sprite entry right away
	int sprite_index = offset / 4;
	uint16_t base_addr = addr & 0xFFFC;
	sprites[sprite_index].DecodeFromOAM(base_addr, oam.Ptr() + sprite_index * 4);
}

void GPU::RenderTilesViz(ColourBuffer* out_buffers, CgbTileAttribute* out_attrs)
{
	static TilePixelRow pixels;
	uint8_t b0;
	uint8_t b1;

	int offset = 0;control.GetTileDataVRamIndex();

	for (int idx = 0; idx < GPU::NumTilesPerSet; idx++)
	{
		int vram_idx = offset + idx * 16;

		CgbTileAttribute& tile_attr = out_attrs[idx];
		tile_attr.DecodeFromByte(bCGB ? vram[vram_idx] : 0);

		int data_idx = vram_idx;
		ColourBuffer& viz_buffer = out_buffers[idx];

		for (int r = 0; r < 8; r++)
		{
			data_idx += r * 2;

			b0 = vram[data_idx];
			b1 = vram[data_idx + 1];
			DecodePixels(pixels, b0, b1, bCGB && tile_attr.HorizontalFlip);

			if (bCGB)
			{
				viz_buffer.SetPixel(0, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[0]));
				viz_buffer.CorrectPixel(0, r);
				viz_buffer.SetPixel(1, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[1]));
				viz_buffer.CorrectPixel(1, r);
				viz_buffer.SetPixel(2, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[2]));
				viz_buffer.CorrectPixel(2, r);
				viz_buffer.SetPixel(3, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[3]));
				viz_buffer.CorrectPixel(3, r);
				viz_buffer.SetPixel(4, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[4]));
				viz_buffer.CorrectPixel(4, r);
				viz_buffer.SetPixel(5, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[5]));
				viz_buffer.CorrectPixel(5, r);
				viz_buffer.SetPixel(6, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[6]));
				viz_buffer.CorrectPixel(6, r);
				viz_buffer.SetPixel(7, r, bgColourPalette.GetColour(tile_attr.Palette, pixels[7]));
				viz_buffer.CorrectPixel(7, r);
			}
			else
			{
				viz_buffer.SetPixel(0, r, bgMonoPalette.GetColour(pixels[0]));
				viz_buffer.SetPixel(1, r, bgMonoPalette.GetColour(pixels[1]));
				viz_buffer.SetPixel(2, r, bgMonoPalette.GetColour(pixels[2]));
				viz_buffer.SetPixel(3, r, bgMonoPalette.GetColour(pixels[3]));
				viz_buffer.SetPixel(4, r, bgMonoPalette.GetColour(pixels[4]));
				viz_buffer.SetPixel(5, r, bgMonoPalette.GetColour(pixels[5]));
				viz_buffer.SetPixel(6, r, bgMonoPalette.GetColour(pixels[6]));
				viz_buffer.SetPixel(7, r, bgMonoPalette.GetColour(pixels[7]));
			}
		}
	}
}

void GPU::RenderSpritesViz(ColourBuffer* out_buffers, SpriteData* out_sprites)
{
	int sprite_height = control.SpriteSize == 0 ? 8 : 16;
	static TilePixelRow pixels;

	for (int i = 0; i < NumSprites; i++)
	{
		const SpriteData& sprite = sprites[i];
		out_sprites[i] = sprite;


		ColourBuffer& curr_tile = out_buffers[i];
		curr_tile.Fill(GemColour::White());
		curr_tile.Width = 8;
		curr_tile.Height = sprite_height;

		// 4px padding around the tile within the grid cell
		int tile_draw_xpos = (i % 8) * 19 + 4;
		int tile_draw_ypos = (i / 8) * 34 + 4;

		for (int ln = 0; ln < sprite_height; ln++)
		{
			if (sprite.IsZero)
			{
				curr_tile[8 * ln + 0] = GemColour::White();
				curr_tile[8 * ln + 1] = GemColour::White();
				curr_tile[8 * ln + 2] = GemColour::White();
				curr_tile[8 * ln + 3] = GemColour::White();
				curr_tile[8 * ln + 4] = GemColour::White();
				curr_tile[8 * ln + 5] = GemColour::White();
				curr_tile[8 * ln + 6] = GemColour::White();
				curr_tile[8 * ln + 7] = GemColour::White();
			}
			else
			{
				int pixel_row = ln;
				int tile = sprite.Tile;
				if (sprite_height == 16)
				{
					if (pixel_row >= 8)
					{
						tile |= 0x01;
						pixel_row -= 8;
					}
					else
					{
						tile &= 0xFE;
					}
				}

				int tile_data_vram_index = (tile * 16) + (pixel_row * 2);

				uint8_t b0 = 0;
				uint8_t b1 = 0;
				if (bCGB && sprite.VRamBank == 1)
				{
					b0 = vram[0x2000 + tile_data_vram_index];
					b1 = vram[0x2000 + tile_data_vram_index + 1];
				}
				else
				{
					b0 = vram[tile_data_vram_index];
					b1 = vram[tile_data_vram_index + 1];
				}

				// Early out for colour number 0 which is always transparent
				if (b0 == 0 && b1 == 0)
					continue;

				DecodePixels(pixels, b0, b1, false);

				if (bCGB)
				{
					curr_tile[8 * ln + 0] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[0]);
					curr_tile[8 * ln + 0].Correct();
					curr_tile[8 * ln + 1] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[1]);
					curr_tile[8 * ln + 1].Correct();
					curr_tile[8 * ln + 2] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[2]);
					curr_tile[8 * ln + 2].Correct();
					curr_tile[8 * ln + 3] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[3]);
					curr_tile[8 * ln + 3].Correct();
					curr_tile[8 * ln + 4] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[4]);
					curr_tile[8 * ln + 4].Correct();
					curr_tile[8 * ln + 5] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[5]);
					curr_tile[8 * ln + 5].Correct();
					curr_tile[8 * ln + 6] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[6]);
					curr_tile[8 * ln + 6].Correct();
					curr_tile[8 * ln + 7] = sprColourPalette.GetColour(sprite.CGBPalette, pixels[7]);
					curr_tile[8 * ln + 7].Correct();
				}
				else
				{
					curr_tile[8 * ln + 0] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[0]);
					curr_tile[8 * ln + 1] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[1]);
					curr_tile[8 * ln + 2] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[2]);
					curr_tile[8 * ln + 3] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[3]);
					curr_tile[8 * ln + 4] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[4]);
					curr_tile[8 * ln + 5] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[5]);
					curr_tile[8 * ln + 6] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[6]);
					curr_tile[8 * ln + 7] = sprMonoPalettes[sprite.DMGPalette].GetColour(pixels[7]);
				}
			}
		}
	}
}

void GPU::RenderPalettesViz(ColourBuffer* out_buffers, ColourPalette::PaletteEntry* out_entries)
{
	for (int idx = 0; idx < NumPaletteColours; idx++)
	{
		ColourPalette::PaletteEntry& entry = idx < 32 ? bgColourPalette.Data[idx] : sprColourPalette.Data[idx - 32];
		GemColour colour = entry.Colour;
		colour.Correct();

		out_buffers[idx].Fill(colour);
		out_entries[idx] = entry;
	}
}

//////////////////////////////
///     Tile Pixel Row     ///
//////////////////////////////

TilePixelRow::TilePixelRow()
{
}

TilePixelRow::TilePixelRow(const int* arr)
{
	p0 = arr[0];
	p1 = arr[1];
	p2 = arr[2];
	p3 = arr[3];
	p4 = arr[4];
	p5 = arr[5];
	p6 = arr[6];
	p7 = arr[7];
}

uint8_t* TilePixelRow::Ptr()
{
	return reinterpret_cast<uint8_t*>(this);
}

uint8_t TilePixelRow::operator[](int index)
{
	switch (index)
	{
	case 0:
		return p0;
	case 1:
		return p1;
	case 2:
		return p2;
	case 3:
		return p3;
	case 4:
		return p4;
	case 5:
		return p5;
	case 6:
		return p6;
	case 7:
		return p7;
	}

	throw std::exception("Invalid index");
}

void TilePixelRow::CopyToArray(int* arr)
{
	arr[0] = p0;
	arr[1] = p1;
	arr[2] = p2;
	arr[3] = p3;
	arr[4] = p4;
	arr[5] = p5;
	arr[6] = p6;
	arr[7] = p7;
}

/*
Rough explanation of how graphics data is arranged and rendered:

A tile is 16 bytes. Tile data is contained within the range 8000h-97FFh (6144 bytes). This makes
enough room for 384 tiles. There are 2 virtual tile sets (512 tiles) that share this space. Depending
on which one is selected (via bit 4 of FF40h), this is how they're laid out:

Tile set #0:
8000h-87FFh: N/A
8800h-8FFFh: -1 to -128
9000h-97FFh: 0 to 127


Tile set #1:
8000h-87FFh: 0-127
8800h-8FFFh: 128-255
9000h-97FFh: N/A

There are 2 tile maps that are used to "draw" a screen/background. Map 1 is in the range 9800h-9BFFh.
Map 2 is in 9C00h-9FFFh. Each map consists of 1024 bytes. Each byte references 1 of 256 unique tiles
from a tile set. The maps are arranged as 32x32 tile numbers. This comes out to 256x256 pixels.

In order to render a given horizontal line (32 tiles wide) in this map, the following needs to happen:
1. Adjust the line with respect to the SCY register (i.e. the Y position of the line).
2. For each of the 32 tiles on this line, figure out where in the appropriate tile map the tile number is.
Remember that every 32 bytes, the map begins referencing a new line.
3. Retrieve the tile number from the tile map and then retrieve its data from the appropriate tile set.
4. The tile is 8x8 pixels. So figure out which horizontal line of pixels you need to render.
5. Decode the pixels via the current palette and render it.

In CGB mode, this layout is slightly modified. Inside Bank 1 of the VRAM there is a 32x32 byte tile
attribute map. It is located in the same memory range as the tile maps, except in another bank. Whereas
the tile maps give us tile numbers, the tile attribute map gives us some special attributes corresponding
to the that tile number. Bits 0-2 correspond to a colour palette. And bit 3 corresponds to one of the VRAM banks.
Thus in CGB mode, the number of tiles that can be referenced is doubled via 2 banks. But the number of tile maps
is the same, the extra bank. And also, CGB mode affords many more colours.

Colour palettes:
As mentioned, there are 16 bytes per 8x8px tile. This leaves 2bits to define 4 pixel colours. The colours
are stored in something called a palette which basically maps this 2bit number to a colour. For a
regular gameboy, the palette maps 4 numbers to 4 colours and is located at FF47h. Simply by changing
the mapping you can change all the colours on screen (e.g. to invert all the colours).

In CGB mode the 3bit colour palette number, in the tile attribute map, indicates 1 of 8 possible
colour palletes. Each pallete can contain 4 colours for a total of 32 colors on screen for the
background. Palettes are not directly mapped to the address space; they are contained inside
the gameboy's internal boot ROM. In order to read/write to the pallete, one must first set the
index of the palette using register FF68h, then either read from or write to the palette data in
register FF69h. This register is 15bits. Each 5 bits indicate red, green or blue intensity.


The LCD controller's state/mode transition logic can be treated like a finite state machine. 
The ascii diagram below shows how the states (LCDMode) can transition. (2) transitions to (3). (3) transitions to (0).
(0) either transitions to (1) or (2) and (1) transitions to (2). All of them can transition to themselves.

 ________              ____________            ________           ___________
V        \            V            \          V        \         V           \
{ReadingOam(2)}---->{ReadingVRam(3)}------>{HBlank(0)}----->{VBlank(1)}---|
^                                               /                 /
\______________________________________________/_________________/

*/
