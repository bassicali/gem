
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <string>

#include <windows.h>
#include <compressapi.h>

#include "Colour.h"
#include "Core/MMU.h"
#include "Core/CartridgeReader.h"

class Gem;
class AVCodecContext;
class AVFrame;
class AVPacket;

template <class TPtr>
struct MovablePointer
{
	TPtr* Ptr;

	MovablePointer()
		: Ptr(nullptr)
	{
	}

	MovablePointer(const TPtr* p)
	{
		Ptr = p;
	}

	MovablePointer(MovablePointer<TPtr>&& other)
	{
		Ptr = other.Ptr;
		other.Ptr = nullptr;
	}

	MovablePointer<TPtr>& operator=(MovablePointer<TPtr>&& other)
	{
		Ptr = other.Ptr;
		other.Ptr = nullptr;
	}
};

struct RewindSnapshot
{
	int Size() const;
	int CompressedFrameSize() const;

	//void RewindSnapshot::MoveTo(RewindSnapshot& other);

	RewindSnapshot();
	~RewindSnapshot();

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
	std::vector<uint8_t>		MMU_CompressedWRAM;
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
	std::vector<uint8_t>		MBC_CompressedExtRAM;

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
	std::vector<uint8_t>		GPU_CompressedSprites;
	std::vector<uint8_t>		GPU_CompressedVRAM;

	AVPacket*					GPU_CompressedFramePacket;

	// APU
	// todo...

};

typedef DArray<RewindSnapshot> RewindBuffer;

class RewindManager
{
public:

	RewindManager();
	RewindManager(Gem* core, int count);
	~RewindManager();

	void SetCore(Gem* core);
	bool InitVideoCodec();
	void Shutdown();
	bool IsInitialized() const { return initialized; }

	void RecordSnapshot();
	void ClearBuffer();
	void ResizeBuffer(int count);
	int GetBufferSize() const { return totalBufferSize; }

	bool StartRewind();
	void ApplyCurrentRewindSnapshot();
	void GetCurrentRewindFrame(ColourBuffer& framebuffer);
	bool StopRewind(bool continueFromStart);
	bool IsRewinding() const { return isRewinding; }

	std::string GetStatsSummary() const;

private:
	
	void ApplySnapshot(const RewindSnapshot& snapshot);
	void GetSnapshot(RewindSnapshot& snapshot);

	bool CompressData(const uint8_t* data, int len, std::vector<uint8_t>& output_buffer);
	bool DecompressData(const uint8_t* data, int len, std::vector<uint8_t>& output_buffer);

	bool EncodeVideoFrame(const ColourBuffer& framebuffer, AVPacket*& output_packet);
	bool DecodeVideoFrame(const AVPacket* packet, ColourBuffer& output_buffer);

	bool initialized = false;
	bool shutdown = false;

	Gem* core;

	COMPRESSOR_HANDLE compressor = nullptr;
	DECOMPRESSOR_HANDLE decompressor = nullptr;

	// Circular buffer
	int idxHead = -1;
	int idxTail = -1;
	int idxNext = 0;
	int buffCount = 0;
	std::vector<std::optional<RewindSnapshot>> buffer;
	int totalBufferSize = 0;

	bool isRewinding = false;
	int idxRewind = 0;

	// If user wants to cancel rewind we restore this snapshot which is when they began rewinding
	RewindSnapshot rewindUndoSnapshot;

	std::vector<uint8_t> joinedWorkingRAM;
	std::vector<uint8_t> joinedExtRAM;
	std::vector<uint8_t> joinedVRAM;
	std::vector<uint8_t> joinedSprites;

	// ffmpeg stuff
	AVCodecContext* vidEncoder = nullptr;
	AVCodecContext* vidDecoder = nullptr;
	AVFrame* vidFrame = nullptr;
};