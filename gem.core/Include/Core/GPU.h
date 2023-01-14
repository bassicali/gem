#pragma once

#include <memory>
#include <cstdint>
#include <string>

#include "Core/GPURegisters.h"
#include "Core/CartridgeReader.h"
#include "Core/InterruptController.h"
#include "DArray.h"
#include "Colour.h"
#include "IDrawTarget.h"
#include "IMappedComponent.h"

struct TilePixelRow
{
	TilePixelRow();
	TilePixelRow(const int* arr);
	uint8_t* Ptr();
	uint8_t operator[](int index);
	void CopyToArray(int* arr);

	uint8_t p0;
	uint8_t p1;
	uint8_t p2;
	uint8_t p3;
	uint8_t p4;
	uint8_t p5;
	uint8_t p6;
	uint8_t p7;
};

class GPU
{
	public:
		GPU();
		void Reset(bool bCGB);
		void TickStateMachine(int t_cycles);

		uint8_t ReadByteVRAM(uint16_t addr);
		uint8_t ReadByteOAM(uint16_t addr);

		void WriteDWordOAM(uint16_t addr, uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3);
		void WriteByteOAM(uint16_t addr, uint8_t value); // This write also updates the sprites vector
		void WriteByteVRAM(uint16_t addr, uint8_t value);

		void WriteVRAMBank(uint8_t value);
		int ReadVRAMBank() const { return vramBank; }

		void WriteRegister(uint16_t addr, uint8_t value);
		uint8_t ReadRegister(uint16_t addr);

		ColourPalette& GetBgColourPalette() { return bgColourPalette; }
		ColourPalette& GetSpriteColourPalette() { return sprColourPalette; }
		MonochromePalette& GetBgMonochromePalette() { return bgMonoPalette; }
		MonochromePalette& GetSpriteMonochromePalette(int index) { return sprMonoPalettes[index]; }

		LCDPositions& GetLCDPositions() { return positions; }
		LCDStatusRegister& GetLCDStatus() { return stat; }
		LCDControlRegister& GetLCDControl() { return control; }
		void SetLCDControl(uint8_t value);

		void SetCartridge(std::shared_ptr<CartridgeReader> ptr);
		const bool IsCGB() const { return bCGB; }
		void SetInterruptController(std::shared_ptr<InterruptController> ptr);
		void SetMMU(std::shared_ptr<IMMU> ptr);

		LCDMode GetMode() const { return stat.Mode; }
		void SetMode(LCDMode mode) { stat.Mode = mode; }

		const ColourBuffer& GetFrameBuffer() { return frameBuffer; }
		void ClearFrameBuffer() { frameBuffer.Zero(); }

		void RenderSpritesViz(ColourBuffer* out_buffers, SpriteData* out_sprites);
		void RenderTilesViz(int tile_set, ColourBuffer* out_buffers, CGBTileAttribute* out_attrs, uint16_t* addrs);
		void RenderPalettesViz(ColourBuffer* out_buffers, ColourPalette::PaletteEntry* out_entries);

		CorrectionMode GetColourCorrectionMode() const { return correctionMode; }
		void SetColourCorrectionMode(CorrectionMode mode) { correctionMode = mode; }

		float GetBrightness() const { return brightness; }
		void SetBrightness(float value) { brightness = value; }

		// Even if running a DMG-only game, we reserve the extra bank
		static const int VRAMSize = 0x2000 * 2; // 8kb * 2 banks
		static const int OAMSize = 0xA0; // 160 bytes (4bytes per sprite)
		static const int NumSprites = 40;
		static const int NumTilesPerSet = 256;
		static const int NumPaletteColours = 64;

		static const int LCDWidth = 160;
		static const int LCDHeight = 144;
		static const int TileMapWidth = 256;
		static const int TileMapHeight = 256;

		static const int PalettesViewWidth = 83;
		static const int PalettesViewHeight = 125;

	private:
		bool bCGB;
		int tAcc; // Accumulates T cycles

		void RenderLine();
		void RenderBGLine();
		void RenderWindowLine();
		void RenderSpriteLine();
		void GetTilePixelRow(int line_pos, TilePixelRow& pixels, CGBTileAttribute& tile_attr);
		void GetWindowTilePixelRow(int line_pos, TilePixelRow& pixels, CGBTileAttribute& tile_attr);
		void ReadPixels(TilePixelRow& pixels, int vram_index, bool read_bank_1, bool horizontal_flip);
		void DecodePixels(TilePixelRow& pixels, uint8_t b0, uint8_t b1, bool horizontal_flip);

		void IncLineY();
		void LycLyCompare();

		int vramBank;
		int vramOffset;
		DArray<uint8_t> vram; 
		DArray<uint8_t> oam;
		SpriteData sprites[NumSprites];

		DMATransferRegisters dma;
		uint16_t dmaSrc;
		uint16_t dmaDest;
		void DmaTransferToOam(uint8_t source);

		LCDControlRegister control;
		LCDPositions positions;
		LCDStatusRegister stat;
		MonochromePalette bgMonoPalette;
		MonochromePalette sprMonoPalettes[2];
		ColourPalette bgColourPalette;
		ColourPalette sprColourPalette;

		ColourBuffer frameBuffer;
		CorrectionMode correctionMode;
		float brightness;

		std::shared_ptr<CartridgeReader> cart;
		std::shared_ptr<InterruptController> interrupts;
		std::shared_ptr<IMMU> mmu;

		friend class RewindManager;
};
