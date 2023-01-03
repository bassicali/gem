
#pragma once

#include <cstdint>
#include <windows.h>
#include <compressapi.h>

#include "DArray.h"
#include "Colour.h"
#include "Core/MMU.h"
#include "Core/CartridgeReader.h"

class Gem;
class AVCodecContext;
class AVFrame;
class AVPacket;

struct RewindSnapshot
{
	int Size();

	bool						Gem_bCGB;

	// Z80 state
	uint8_t						Z80_registerFile[8];
	uint16_t					Z80_PC;
	uint16_t					Z80_SP;
	bool						Z80_bCGB;
	bool						Z80_interruptMasterEnable;
	bool						Z80_isStopped;
	bool						Z80_isHalted;

	// InterruptCtl
	bool						IRCtl_VBlankEnabled;
	bool						IRCtl_LCDStatusEnabled;
	bool						IRCtl_TimerEnabled;
	bool						IRCtl_SerialEnabled;
	bool						IRCtl_JoypadEnabled;
	uint8_t						IRCtl_EnableRegisterByte;
	bool						IRCtl_VBlankRequested;
	bool						IRCtl_LCDStatusRequested;
	bool						IRCtl_TimerRequested;
	bool						IRCtl_SerialRequested;
	bool						IRCtl_JoypadRequested;

	// MMU
	DataBuffer					MMU_CompressedWRAM;
	bool						MMU_bCGB;
	uint8_t						MMU_hram[128];

	// MBC
	CartridgeProperties			MBC_cp;
	BankingMode					MBC_bankingMode;
	uint16_t					MBC_romBank;
	int							MBC_romOffset;
	bool						MBC_exRAMEnabled;
	uint16_t					MBC_extRAMBank;
	int							MBC_numExtRAMBanks;
	bool						MBC_rtcEnabled;
	bool						MBC_zeroSeen;
	int							MBC_latchedRTCRegister;
	int							MBC_dayCtr;
	tm							MBC_lastLatchedTime;
	bool						MBC_daysOverflowed;
	DataBuffer					MBC_CompressedExtRAM;

	// CGBRegisters
	int							CGBReg_wramBank;
	uint16_t					CGBReg_wramOffset;
	bool						CGBReg_prepareSpeedSwitch;
	SpeedMode					CGBReg_currentSpeed;

	// TimerController
	uint8_t						TmrCtl_Divider;
	uint8_t						TmrCtl_Counter;
	uint8_t						TmrCtl_Modulo;
	uint8_t						TmrCtl_Speed;
	uint8_t						TmrCtl_TACRegisterByte;
	bool						TmrCtl_Running;
	bool						TmrCtl_bCGB;
	int							TmrCtl_divAcc;
	int							TmrCtl_ctrAcc;
	uint32_t					TmrCtl_tCyclesPerCtrCycle;
	
	// SerialController
	bool						SerialCtl_TxStart;
	int							SerialCtl_ShiftClockType;
	uint8_t						SerialCtl_RegisterByte;

	// GPU
	bool						GPU_bCGB;
	int							GPU_tAcc;
	int							GPU_vramBank;
	int							GPU_vramOffset;
	DMATransferRegisters		GPU_dma;
	uint16_t					GPU_dmaSrc;
	uint16_t					GPU_dmaDest;
	LCDControlRegister			GPU_control;
	LCDPositions				GPU_positions;
	LCDStatusRegister			GPU_stat;
	MonochromePalette			GPU_bgMonoPalette;
	MonochromePalette			GPU_sprMonoPalettes[2];
	ColourPalette				GPU_bgColourPalette;
	ColourPalette				GPU_sprColourPalette;
	CorrectionMode				GPU_correctionMode;
	float						GPU_brightness;
	uint8_t						GPU_oam[160];
	DataBuffer					GPU_CompressedSprites;
	DataBuffer					GPU_CompressedVRAM;

	AVPacket*					GPU_CompressedFramePacket;

	// APU
	// todo...

};

typedef DArray<RewindSnapshot> RewindBuffer;

class RewindManager
{
public:

	RewindManager();
	RewindManager(Gem* core, float durationSec);
	~RewindManager();

	void SetCore(Gem* core);
	bool InitVideoCodec();
	void Shutdown();

	bool StartRecording();
	void RecordSnapshot();
	void StopRecording();
	bool IsRecording() const { return isRecording; }
	void ClearBuffer();

	bool StartPlayback();
	void ApplyCurrentSnapshot();
	void GetCurrentPlaybackFrame(ColourBuffer& framebuffer);
	bool StopPlayback(bool continueFromStart);
	bool IsPlaying() const { return isPlaying; }
	void SetBufferSize(float durationSeconds);

	const RewindSnapshot& SavePoint() const { return savePoint; }

private:
	
	void ApplySnapshot(RewindSnapshot& snapshot);
	RewindSnapshot GetSnapshot();

	bool AddToBuffer(const RewindSnapshot& snapshot);
	bool CompressData(const uint8_t* data, int len, DataBuffer& output_buffer);
	bool DecompressData(const uint8_t* data, int len, DataBuffer& output_buffer);

	bool EncodeVideoFrame(const ColourBuffer& framebuffer, AVPacket*& output_packet);
	bool DecodeVideoFrame(const AVPacket* packet, ColourBuffer& output_buffer);

	Gem* core;

	COMPRESSOR_HANDLE compressor = nullptr;
	DECOMPRESSOR_HANDLE decompressor = nullptr;

	bool isRecording = false;
	bool isPlaying = false;
	int idxBuffPlay = 0;

	// Circular buffer
	int idxBuffStart = 0;
	int idxBuffEnd = 0;
	RewindBuffer buffer;
	RewindSnapshot savePoint;
	AVPacket* savePointPacket = nullptr;

	DataBuffer joinedWorkingRAM;
	DataBuffer joinedExtRAM;
	DataBuffer joinedVRAM;
	DataBuffer joinedOAM;
	DataBuffer joinedSprites;

	// ffmpeg stuff
	AVCodecContext* videoCtx = nullptr;
	AVFrame* vidFrame = nullptr;
};