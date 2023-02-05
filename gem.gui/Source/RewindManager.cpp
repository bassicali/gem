
#include "RewindManager.h"
#include "Logging.h"
#include "Core/Gem.h"
#include "GemConfig.h"

#include <sstream>
#include <cmath>
#include <cassert>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
}

using namespace std;

////////////////////////
///   RewindManager   //
////////////////////////
RewindManager::RewindManager()
	: RewindManager(nullptr, 300)
{
}

RewindManager::RewindManager(Gem* core, int count)
	: joinedWorkingRAM(8 * 0x1000)
	, joinedExtRAM(4 * 0x2000)
	, joinedVRAM(2 * 0x2000)
	, joinedSprites(40 * sizeof(SpriteData))
	, buffer(count)
	, core(core)
{
}

bool RewindManager::InitVideoCodec()
{
	if (shutdown)
		return false;

	int error_code = 0;

	bool encoder_init = false;
	bool decoder_init = false;

	if (const AVCodec* pencoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG))
	{
		if (vidEncoder = avcodec_alloc_context3(pencoder))
		{
			vidEncoder->width = GPU::LCDWidth;
			vidEncoder->height = GPU::LCDHeight;
			vidEncoder->time_base = { 1, 30 };
			vidEncoder->framerate = { 30, 1 };
			//vidEncoder->gop_size = 1;
			vidEncoder->max_b_frames = 0;
			vidEncoder->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUVJ444P;

			if ((error_code = avcodec_open2(vidEncoder, pencoder, nullptr)) >= 0)
			{
				encoder_init = true;
			}
			else
			{
				avcodec_free_context(&vidEncoder);

				char errstr[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(error_code, errstr, AV_ERROR_MAX_STRING_SIZE);
				LOG_ERROR("Failed to initialize video encoder for rewind. Error: %s", errstr);
			}
		}
	}

	if (!encoder_init)
	{
		return false;
	}

	if (const AVCodec* pdecoder = avcodec_find_decoder(AV_CODEC_ID_MJPEG))
	{
		if (vidDecoder = avcodec_alloc_context3(pdecoder))
		{
			vidDecoder->width = GPU::LCDWidth;
			vidDecoder->height = GPU::LCDHeight;
			vidDecoder->time_base = { 1, 30 };
			vidDecoder->framerate = { 30, 1 };
			vidDecoder->max_b_frames = 0;
			vidDecoder->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUVJ444P;

			if ((error_code = avcodec_open2(vidDecoder, pdecoder, nullptr)) >= 0)
			{
				decoder_init = true;
			}
			else
			{
				avcodec_free_context(&vidDecoder);

				char errstr[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(error_code, errstr, AV_ERROR_MAX_STRING_SIZE);
				LOG_ERROR("Failed to initialize video decoder for rewind. Error: %s", errstr);
			}
		}
	}

	initialized = encoder_init && decoder_init;
	return initialized;
}

void RewindManager::SetCore(Gem* ptr)
{
	core = ptr;
}

RewindManager::~RewindManager()
{
	Shutdown();
}

void RewindManager::Shutdown()
{
	if (shutdown)
		return;
	
	ClearBuffer();

	if (vidEncoder)
	{
		avcodec_free_context(&vidEncoder);
	}

	if (vidFrame)
	{
		av_frame_free(&vidFrame);
	}

	if (compressor)
	{
		CloseCompressor(compressor);
	}

	if (decompressor)
	{
		CloseDecompressor(decompressor);
	}

	shutdown = true;
}

bool RewindManager::LoadFromGemSave(std::istream& input, std::streamsize& count)
{
	RewindSnapshot snapshot;
	if (snapshot.ReadFromStream(input, count))
	{
		ApplySnapshot(snapshot);
		return true;
	}
	return false;
}

bool RewindManager::WriteToGemSave(std::ostream& output, std::streamsize& count)
{
	RewindSnapshot snapshot;
	GetSnapshot(snapshot, /*include_frame_buffer*/ false);
	return snapshot.WriteToStream(output, count);
}

void RewindManager::RecordSnapshot()
{
	if (isRewinding || !initialized)
	{
		return;
	}

	// If the spot is full erase/destroy it and create a new one
	// TODO: consider recycling the snapshot instead
	if (buffer[idxNext])
	{
		totalBufferSize -= buffer[idxNext]->Size();
		buffer[idxNext].reset();
	}

	buffer[idxNext] = RewindSnapshot();
	GetSnapshot(*buffer[idxNext]);

	assert(buffCount <= buffer.size());
	assert((buffCount == buffer.size()) == (idxHead > idxTail || (idxHead == 0 && idxTail == buffer.size()-1)));

	totalBufferSize += buffer[idxNext]->Size();

	if (buffCount == 0)
	{
		idxHead = 0;
		idxTail = 0;
		idxNext = 1;
		buffCount++;
	}
	else if (buffCount == 1)
	{
		idxTail = 1;
		idxNext = 2;
		buffCount++;
	}
	else if (buffCount < buffer.size())
	{
		idxTail = (idxTail + 1) % buffer.size();
		idxNext = (idxNext + 1) % buffer.size();
		buffCount++;
	}
	else // The buffer is full
	{
		assert(buffer.size() == buffCount);

		idxTail = (idxTail + 1) % buffCount;
		idxHead = (idxHead + 1) % buffCount;
		idxNext = idxHead;
	}
}

void RewindManager::GetSnapshot(RewindSnapshot& snapshot, bool include_frame_buffer /* = true*/)
{
	snapshot.Gem_bCGB = core->bCGB;

	// Z80 state
	Z80& cpu = core->cpu;
	snapshot.Z80_PC = cpu.PC;
	snapshot.Z80_SP = cpu.SP;
	snapshot.Z80_bCGB = cpu.bCGB;
	snapshot.Z80_interruptMasterEnable = cpu.interruptMasterEnable;
	snapshot.Z80_isStopped = cpu.isStopped;
	snapshot.Z80_isHalted = cpu.isHalted;
	
	for (int i = 0; i < 8; i++)
		snapshot.Z80_registerFile[i] = cpu.registerFile[i];


	// InterruptCtl
	auto irctl = core->mmu->interrupts;
	snapshot.IRCtl_VBlankEnabled = irctl->VBlankEnabled;
	snapshot.IRCtl_LCDStatusEnabled = irctl->LCDStatusEnabled;
	snapshot.IRCtl_TimerEnabled = irctl->TimerEnabled;
	snapshot.IRCtl_SerialEnabled = irctl->SerialEnabled;
	snapshot.IRCtl_JoypadEnabled = irctl->JoypadEnabled;
	snapshot.IRCtl_EnableRegisterByte = irctl->EnableRegisterByte;
	snapshot.IRCtl_VBlankRequested = irctl->VBlankRequested;
	snapshot.IRCtl_LCDStatusRequested = irctl->LCDStatusRequested;
	snapshot.IRCtl_TimerRequested = irctl->TimerRequested;
	snapshot.IRCtl_SerialRequested = irctl->SerialRequested;
	snapshot.IRCtl_JoypadRequested = irctl->JoypadRequested;


	// MMU
	auto mmu = core->mmu;
	snapshot.MMU_bCGB = mmu->bCGB;
	
	for (int i = 0; i < 128; i++)
		snapshot.MMU_hram[i] = mmu->hram[i];
	

	for (int i = 0; i < 8; i++)
	{
		memcpy(joinedWorkingRAM.data() + 0x1000 * i, mmu->wramBanks[i].Ptr(), mmu->wramBanks[i].Size());
	}

	snapshot.MMU_CompressedWRAM = vector<uint8_t>(0x1000);
	CompressData(joinedWorkingRAM.data(), joinedWorkingRAM.size(), snapshot.MMU_CompressedWRAM);

	// MBC
	MBC& mbc = core->mmu->mbc;
	snapshot.MBC_cp = mbc.cp;
	snapshot.MBC_bankingMode = mbc.bankingMode;
	snapshot.MBC_romBank = mbc.romBank;
	snapshot.MBC_romOffset = mbc.romOffset;
	snapshot.MBC_exRAMEnabled = mbc.exRAMEnabled;
	snapshot.MBC_extRAMBank = mbc.extRAMBank;
	snapshot.MBC_numExtRAMBanks = mbc.numExtRAMBanks;
	snapshot.MBC_rtcEnabled = mbc.rtcEnabled;
	snapshot.MBC_zeroSeen = mbc.zeroSeen;
	snapshot.MBC_latchedRTCRegister = mbc.latchedRTCRegister;
	snapshot.MBC_dayCtr = mbc.dayCtr;
	snapshot.MBC_lastLatchedTime = mbc.lastLatchedTime;
	snapshot.MBC_daysOverflowed = mbc.daysOverflowed;

	int ext_ram_size = 0;
	for (int i = 0; i < mbc.numExtRAMBanks; i++)
	{
		memcpy(joinedExtRAM.data() + MBC::RAMBankSize * i, mbc.extRAMBanks[i].Ptr(), mbc.extRAMBanks[i].Size());
		ext_ram_size += MBC::RAMBankSize;
	}

	snapshot.MBC_CompressedExtRAM = vector<uint8_t>(0x1000);
	CompressData(joinedExtRAM.data(), ext_ram_size, snapshot.MBC_CompressedExtRAM);

	// CGBRegisters
	CGBRegisters& cgb = core->mmu->cgb_state;
	snapshot.CGBReg_wramBank = cgb.wramBank;
	snapshot.CGBReg_wramOffset = cgb.wramOffset;
	snapshot.CGBReg_prepareSpeedSwitch = cgb.prepareSpeedSwitch;
	snapshot.CGBReg_currentSpeed = cgb.currentSpeed;


	// TimerController
	TimerController& tmr = core->mmu->timer;
	snapshot.TmrCtl_Divider = tmr.Divider;
	snapshot.TmrCtl_Counter = tmr.Counter;
	snapshot.TmrCtl_Modulo = tmr.Modulo;
	snapshot.TmrCtl_Speed = tmr.Speed;
	snapshot.TmrCtl_TACRegisterByte = tmr.TACRegisterByte;
	snapshot.TmrCtl_Running = tmr.Running;
	snapshot.TmrCtl_bCGB = tmr.bCGB;
	snapshot.TmrCtl_divAcc = tmr.divAcc;
	snapshot.TmrCtl_ctrAcc = tmr.ctrAcc;
	snapshot.TmrCtl_tCyclesPerCtrCycle = tmr.tCyclesPerCtrCycle;


	// SerialController
	SerialController& serial = core->mmu->serial;
	snapshot.SerialCtl_TxStart = serial.TxStart;
	snapshot.SerialCtl_ShiftClockType = serial.ShiftClockType;
	snapshot.SerialCtl_RegisterByte = serial.RegisterByte;


	// GPU
	auto gpu = core->gpu;
	snapshot.GPU_bCGB = gpu->bCGB;
	snapshot.GPU_tAcc = gpu->tAcc;
	snapshot.GPU_vramBank = gpu->vramBank;
	snapshot.GPU_vramOffset = gpu->vramOffset;
	snapshot.GPU_dma = gpu->dma;
	snapshot.GPU_dmaSrc = gpu->dmaSrc;
	snapshot.GPU_dmaDest = gpu->dmaDest;
	snapshot.GPU_control = gpu->control;
	snapshot.GPU_positions = gpu->positions;
	snapshot.GPU_stat = gpu->stat;
	snapshot.GPU_bgMonoPalette = gpu->bgMonoPalette;
	snapshot.GPU_sprMonoPalettes[0] = gpu->sprMonoPalettes[0];
	snapshot.GPU_sprMonoPalettes[1] = gpu->sprMonoPalettes[1];
	snapshot.GPU_bgColourPalette = gpu->bgColourPalette;
	snapshot.GPU_sprColourPalette = gpu->sprColourPalette;
	snapshot.GPU_correctionMode = gpu->correctionMode;
	snapshot.GPU_brightness = gpu->brightness;



	///////
	// VRAM
	memcpy(joinedVRAM.data(), gpu->vram.Ptr(), gpu->vram.Size());
	CompressData(joinedVRAM.data(), joinedVRAM.size(), snapshot.GPU_CompressedVRAM);

	///////
	// OAM
	for (int i = 0; i < GPU::OAMSize; i++)
		snapshot.GPU_oam[i] = gpu->oam[i];

	///////
	// Sprites
	for (int i = 0; i < GPU::NumSprites; i++)
	{
		auto* src = joinedSprites.data() + i * sizeof(SpriteData);
		auto* dst = &(gpu->sprites[i]);
		auto  cnt = GPU::NumSprites * sizeof(SpriteData);

		memcpy(joinedSprites.data() + i * sizeof(SpriteData), 
				&(gpu->sprites[i]), 
				sizeof(SpriteData));
	}

	CompressData(joinedSprites.data(), joinedSprites.size(), snapshot.GPU_CompressedSprites);

	if (include_frame_buffer)
	{
		AVPacket* packet = nullptr;
		EncodeVideoFrame(core->gpu->GetFrameBuffer(), packet);
		snapshot.GPU_CompressedFramePacket = packet;
	}
	else
	{
		snapshot.GPU_CompressedFramePacket = nullptr;
	}
}

void RewindManager::ApplyCurrentRewindSnapshot()
{
	if (!initialized)
	{
		return;
	}

	ApplySnapshot(buffer[idxRewind].value());
}

void RewindManager::ApplySnapshot(const RewindSnapshot& snapshot)
{
	core->bCGB = snapshot.Gem_bCGB;

	// Z80 state
	//Z80_registerFile
	Z80& cpu = core->cpu;
	cpu.PC = snapshot.Z80_PC;
	cpu.SP = snapshot.Z80_SP;
	cpu.bCGB = snapshot.Z80_bCGB;
	cpu.interruptMasterEnable = snapshot.Z80_interruptMasterEnable;
	cpu.isStopped = snapshot.Z80_isStopped;
	cpu.isHalted = snapshot.Z80_isHalted;

	for (int i = 0; i < 8; i++)
		cpu.registerFile[i] = snapshot.Z80_registerFile[i];


	// InterruptCtl
	auto irctl = core->mmu->interrupts;
	irctl->VBlankEnabled = snapshot.IRCtl_VBlankEnabled;
	irctl->LCDStatusEnabled = snapshot.IRCtl_LCDStatusEnabled;
	irctl->TimerEnabled = snapshot.IRCtl_TimerEnabled;
	irctl->SerialEnabled = snapshot.IRCtl_SerialEnabled;
	irctl->JoypadEnabled = snapshot.IRCtl_JoypadEnabled;
	irctl->EnableRegisterByte = snapshot.IRCtl_EnableRegisterByte;
	irctl->VBlankRequested = snapshot.IRCtl_VBlankRequested;
	irctl->LCDStatusRequested = snapshot.IRCtl_LCDStatusRequested;
	irctl->TimerRequested = snapshot.IRCtl_TimerRequested;
	irctl->SerialRequested = snapshot.IRCtl_SerialRequested;
	irctl->JoypadRequested = snapshot.IRCtl_JoypadRequested;


	// MMU
	auto mmu = core->mmu;
	mmu->bCGB = snapshot.MMU_bCGB;

	for (int i = 0; i < 128; i++)
		core->mmu->hram[i] = snapshot.MMU_hram[i];

	DecompressData(snapshot.MMU_CompressedWRAM.data(), snapshot.MMU_CompressedWRAM.size(), joinedWorkingRAM);

	for (int i = 0; i < 8; i++)
	{
		memcpy(mmu->wramBanks[i].Ptr(), joinedWorkingRAM.data() + 0x1000 * i, mmu->wramBanks[i].Capacity());
	}

	// MBC
	MBC& mbc = core->mmu->mbc;
	mbc.cp = snapshot.MBC_cp;
	mbc.bankingMode = snapshot.MBC_bankingMode;
	mbc.romBank = snapshot.MBC_romBank;
	mbc.romOffset = snapshot.MBC_romOffset;
	mbc.exRAMEnabled = snapshot.MBC_exRAMEnabled;
	mbc.extRAMBank = snapshot.MBC_extRAMBank;
	mbc.numExtRAMBanks = snapshot.MBC_numExtRAMBanks;
	mbc.rtcEnabled = snapshot.MBC_rtcEnabled;
	mbc.zeroSeen = snapshot.MBC_zeroSeen;
	mbc.latchedRTCRegister = snapshot.MBC_latchedRTCRegister;
	mbc.dayCtr = snapshot.MBC_dayCtr;
	mbc.lastLatchedTime = snapshot.MBC_lastLatchedTime;
	mbc.daysOverflowed = snapshot.MBC_daysOverflowed;

	DecompressData(snapshot.MBC_CompressedExtRAM.data(), snapshot.MBC_CompressedExtRAM.size(), joinedExtRAM);

	for (int i = 0; i < mbc.numExtRAMBanks; i++)
	{
		memcpy(mbc.extRAMBanks[i].Ptr(), joinedExtRAM.data() + MBC::RAMBankSize * i, MBC::RAMBankSize);
	}

	// CGBRegisters
	CGBRegisters& cgb = core->mmu->cgb_state;
	cgb.wramBank = snapshot.CGBReg_wramBank;
	cgb.wramOffset = snapshot.CGBReg_wramOffset;
	cgb.prepareSpeedSwitch = snapshot.CGBReg_prepareSpeedSwitch;
	cgb.currentSpeed = snapshot.CGBReg_currentSpeed;


	// TimerController
	TimerController& tmr = core->mmu->timer;
	tmr.Divider = snapshot.TmrCtl_Divider;
	tmr.Counter = snapshot.TmrCtl_Counter;
	tmr.Modulo = snapshot.TmrCtl_Modulo;
	tmr.Speed = snapshot.TmrCtl_Speed;
	tmr.TACRegisterByte = snapshot.TmrCtl_TACRegisterByte;
	tmr.Running = snapshot.TmrCtl_Running;
	tmr.bCGB = snapshot.TmrCtl_bCGB;
	tmr.divAcc = snapshot.TmrCtl_divAcc;
	tmr.ctrAcc = snapshot.TmrCtl_ctrAcc;
	tmr.tCyclesPerCtrCycle = snapshot.TmrCtl_tCyclesPerCtrCycle;


	// SerialController
	SerialController& serial = core->mmu->serial;
	serial.TxStart = snapshot.SerialCtl_TxStart;
	serial.ShiftClockType = snapshot.SerialCtl_ShiftClockType;
	serial.RegisterByte = snapshot.SerialCtl_RegisterByte;


	// GPU
	auto gpu = core->gpu;
	gpu->bCGB = snapshot.GPU_bCGB;
	gpu->tAcc = snapshot.GPU_tAcc;
	gpu->vramBank = snapshot.GPU_vramBank;
	gpu->vramOffset = snapshot.GPU_vramOffset;
	gpu->dma = snapshot.GPU_dma;
	gpu->dmaSrc = snapshot.GPU_dmaSrc;
	gpu->dmaDest = snapshot.GPU_dmaDest;
	gpu->control = snapshot.GPU_control;
	gpu->positions = snapshot.GPU_positions;
	gpu->stat = snapshot.GPU_stat;
	gpu->bgMonoPalette = snapshot.GPU_bgMonoPalette;
	gpu->sprMonoPalettes[0] = snapshot.GPU_sprMonoPalettes[0];
	gpu->sprMonoPalettes[1] = snapshot.GPU_sprMonoPalettes[1];
	gpu->bgColourPalette = snapshot.GPU_bgColourPalette;
	gpu->sprColourPalette = snapshot.GPU_sprColourPalette;
	gpu->correctionMode = snapshot.GPU_correctionMode;
	gpu->brightness = snapshot.GPU_brightness;

	DecompressData(snapshot.GPU_CompressedVRAM.data(), snapshot.GPU_CompressedVRAM.size(), joinedVRAM);
	memcpy(gpu->vram.Ptr(), joinedVRAM.data(), GPU::VRAMSize);

	DecompressData(snapshot.GPU_CompressedSprites.data(), snapshot.GPU_CompressedSprites.size(), joinedSprites);
	for (int i = 0; i < GPU::NumSprites; i++)
	{
		memcpy(&(gpu->sprites[i]),
				joinedSprites.data() + i * sizeof(SpriteData),
				sizeof(SpriteData));
	}
}

void RewindManager::ClearBuffer()
{
	int count = buffer.size();
	buffer.clear();
	buffer.resize(count);

	idxHead = 0;
	idxTail = 0;
	idxNext = 0;
	buffCount = 0;
	totalBufferSize = 0;
}

void RewindManager::ResizeBuffer(int count)
{
	buffer.clear();
	buffer.resize(count);

	idxHead = 0;
	idxTail = 0;
	idxNext = 0;
	buffCount = 0;
	totalBufferSize = 0;
}

bool RewindManager::StartRewind()
{
	if (isRewinding || !initialized || buffCount < 2)
	{
		return false;
	}

	GetSnapshot(rewindUndoSnapshot);

	idxRewind = idxTail;
	isRewinding = true;
}

void RewindManager::GetCurrentRewindFrame(ColourBuffer& framebuffer)
{
	if (idxRewind == idxHead) // Reached start of buffer
	{
		return;
	}
	
	DecodeVideoFrame(buffer[idxRewind]->GPU_CompressedFramePacket, framebuffer);

	assert(buffCount <= buffer.size());

	if (buffCount == buffer.size()) // Buffer is full
	{
		idxRewind = idxRewind == 0
					? buffCount - 1
					: idxRewind - 1;
	}
	else
	{
		// Not full means the head index must be the initial value of 0 (note: idxRewind != 0 at this point)
		assert(idxHead == 0);

		// Note the first if statement, idxRewind must be > 0
		idxRewind--;
	}
}

bool RewindManager::StopRewind(bool continueFromStart)
{
	if (!isRewinding || !initialized)
	{
		return false;
	}

	if (continueFromStart)
	{
		ApplySnapshot(rewindUndoSnapshot);
		DecodeVideoFrame(rewindUndoSnapshot.GPU_CompressedFramePacket, core->gpu->frameBuffer);
	}
	else if (GemConfig::Get().RewindClearBufferOnStop)
	{
		ClearBuffer();
	}

	idxRewind = 0;
	isRewinding = false;
}

bool RewindManager::CompressData(const uint8_t* uncompressed_data, int len, vector<uint8_t>& output_buffer)
{
	bool successful = true;

	if (!compressor)
	{
		successful = CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor);
	}
	
	if (successful)
	{
		bool retry = false;
		SIZE_T min_buff_size;

		successful = Compress(compressor, (LPCVOID)uncompressed_data, (SIZE_T)len, output_buffer.data(), output_buffer.size(), &min_buff_size);

		if (!successful)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				output_buffer.resize(min_buff_size);
				retry = true;
			}
		}

		if (retry)
		{
			successful = Compress(compressor, (LPCVOID)uncompressed_data, (SIZE_T)len, output_buffer.data(), output_buffer.size(), &min_buff_size);
		}
	}

	return successful;
}

bool RewindManager::DecompressData(const uint8_t* compressed_data, int len, vector<uint8_t>& output_buffer)
{
	bool successful = true;

	if (!decompressor)
	{
		successful = CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor);
	}

	if (successful)
	{
		bool retry = false;
		SIZE_T min_buff_size;

		successful = Decompress(decompressor, (LPCVOID)compressed_data, (SIZE_T)len, output_buffer.data(), output_buffer.size(), &min_buff_size);

		if (!successful)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				output_buffer.resize(min_buff_size);
				retry = true;
			}
		}

		// If the vector is larger than required we keep the extra slack since we'll be reusing the same snapshot/vector later

		if (retry)
		{
			successful = Decompress(decompressor, (LPCVOID)compressed_data, (SIZE_T)len, output_buffer.data(), output_buffer.size(), &min_buff_size);
		}
	}

	return successful;
}

#pragma region RGB_YUV_conversion_macros
// Source: https://stackoverflow.com/a/14697130/5976386

#define CLIP(X) ( (X) > 255 ? 255 : (X) < 0 ? 0 : X)

#define RGB2Y(R, G, B) CLIP(( (  66 * (R) + 129 * (G) +  25 * (B) + 128) >> 8) +  16)
#define RGB2U(R, G, B) CLIP(( ( -38 * (R) -  74 * (G) + 112 * (B) + 128) >> 8) + 128)
#define RGB2V(R, G, B) CLIP(( ( 112 * (R) -  94 * (G) -  18 * (B) + 128) >> 8) + 128)

#define C(Y) ( (Y) - 16  )
#define D(U) ( (U) - 128 )
#define E(V) ( (V) - 128 )

#define YUV2R(Y, U, V) CLIP(( 298 * C(Y)              + 409 * E(V) + 128) >> 8)
#define YUV2G(Y, U, V) CLIP(( 298 * C(Y) - 100 * D(U) - 208 * E(V) + 128) >> 8)
#define YUV2B(Y, U, V) CLIP(( 298 * C(Y) + 516 * D(U)              + 128) >> 8)
#pragma endregion

bool RewindManager::EncodeVideoFrame(const ColourBuffer& framebuffer, AVPacket*& output_packet)
{
	int error_code = 0;

	if (!vidFrame)
	{
		if (vidFrame = av_frame_alloc())
		{
			vidFrame->format = vidEncoder->pix_fmt;
			vidFrame->width = vidEncoder->width;
			vidFrame->height = vidEncoder->height;
			vidFrame->quality = 1;

			if ((error_code = av_frame_get_buffer(vidFrame, 0)) < 0)
			{
				av_frame_free(&vidFrame);
			}
		}
	}

	if (vidFrame)
	{
		if ((error_code = av_frame_make_writable(vidFrame)) >= 0)
		{
			for (int i = 0; i < framebuffer.Height; i++)
			{
				for (int j = 0; j < framebuffer.Width; j++)
				{
					GemColour px = framebuffer.GetPixel(j, i);
					vidFrame->data[0][i * vidFrame->linesize[0] + j] = RGB2Y(px.Red, px.Green, px.Blue);
					vidFrame->data[1][i * vidFrame->linesize[1] + j] = RGB2U(px.Red, px.Green, px.Blue);
					vidFrame->data[2][i * vidFrame->linesize[2] + j] = RGB2V(px.Red, px.Green, px.Blue);
					//vidFrame->data[3][i * vidFrame->linesize[0] + j] = 255;
				}
			}

			static long frame_timestamp = 0;
			vidFrame->pts = frame_timestamp++;

			if ((error_code = avcodec_send_frame(vidEncoder, vidFrame)) >= 0)
			{
				int count = 0;

				if (AVPacket* packet = av_packet_alloc())
				{
					do
					{
						error_code = avcodec_receive_packet(vidEncoder, packet);
					}
					while (error_code == AVERROR(EAGAIN));

					if (error_code >= 0)
					{
						output_packet = packet;
						return true;
					}
				}
			}
		}
	}

	char errstr[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(error_code, errstr, AV_ERROR_MAX_STRING_SIZE);
	LOG_ERROR("Failed to encode frame. Error: %s", errstr);
	
	return false;
}

bool RewindManager::DecodeVideoFrame(const AVPacket* packet, ColourBuffer& output_buffer)
{
	int error_code = 0;

	if ((error_code = avcodec_send_packet(vidDecoder, packet)) >= 0)
	{
		do 
		{
			error_code = avcodec_receive_frame(vidDecoder, vidFrame);
		} 
		while (error_code == AVERROR(EAGAIN));

		if (error_code >= 0)
		{
			for (int i = 0; i < GPU::LCDHeight; i++)
			{
				for (int j = 0; j < GPU::LCDWidth; j++)
				{
					GemColour px;
					uint8_t y = vidFrame->data[0][i * vidFrame->linesize[0] + j];
					uint8_t u = vidFrame->data[1][i * vidFrame->linesize[1] + j];
					uint8_t v = vidFrame->data[2][i * vidFrame->linesize[2] + j];

					px.Red = YUV2R(y, u, v);
					px.Green = YUV2G(y, u, v);
					px.Blue = YUV2B(y, u, v);
					px.Alpha = 255;
					output_buffer.SetPixel(j, i, px);
				}
			}

			return true;
		}
	}

	char errstr[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(error_code, errstr, AV_ERROR_MAX_STRING_SIZE);
	LOG_ERROR("Failed to decode frame. Error: %s", errstr);

	return false;
}

string RewindManager::GetStatsSummary() const
{
	stringstream ss;

	float ratio_avg = 0;
	float size_avg = 0;
	for (int idx = 0; idx < buffer.size(); idx++)
	{
		if (!buffer[idx])
			continue;

		int size = buffer[idx]->CompressedFrameSize();
		float ratio = float(size) / (160 * 144 * 3);
		ratio_avg += ratio;

		size_avg += float(size);
	}

	ratio_avg = ratio_avg / buffer.size();
	size_avg = size_avg / buffer.size();

	ss << "Buffer size:                     " << (totalBufferSize / 1024) << " kB" << endl;
	ss << "Average frame compression ratio: " << ratio_avg << endl;
	ss << "Average frame compressed size:   " << size_avg << " bytes" << endl;

	return ss.str();
}

///////////////////////
///  RewindSnapshot  //
///////////////////////
RewindSnapshot::RewindSnapshot()
	: GPU_CompressedFramePacket(nullptr)
{
}

RewindSnapshot::~RewindSnapshot()
{
	if (GPU_CompressedFramePacket)
	{
		av_packet_free(&GPU_CompressedFramePacket);
	}
}

int RewindSnapshot::Size() const
{
	int size = sizeof(RewindSnapshot)
				+ MMU_CompressedWRAM.size()
				+ MBC_CompressedExtRAM.size()
				+ GPU_CompressedVRAM.size()
				+ GPU_CompressedSprites.size();

	if (GPU_CompressedFramePacket)
	{
		size += GPU_CompressedFramePacket->size;

		//float ratio = float(GPU_CompressedFramePacket->size) / (160 * 144 * 3);
		//LOG_INFO("Frame compression ratio = %.2f", ratio);
	}

	return size;
}

int RewindSnapshot::CompressedFrameSize() const
{
	return GPU_CompressedFramePacket ? GPU_CompressedFramePacket->size : 0;
}

bool RewindSnapshot::WriteToStream(ostream& output, streamsize& count)
{
#define WRITE(src, size) output.write(reinterpret_cast<char*>(src), size); if (output.good()) { count += size; } else { return false; }
	int size_prefix;

	// Z80 state
	WRITE(&Z80_PC, sizeof(Z80_PC));
	WRITE(&Z80_SP, sizeof(Z80_SP));
	WRITE(&Z80_bCGB, sizeof(Z80_bCGB));
	WRITE(&Z80_interruptMasterEnable, sizeof(Z80_interruptMasterEnable));
	WRITE(&Z80_isStopped, sizeof(Z80_isStopped));
	WRITE(&Z80_isHalted, sizeof(Z80_isHalted));
	WRITE(&Z80_registerFile[0], 8);

	// InterruptCtl
	WRITE(&IRCtl_VBlankEnabled, sizeof(IRCtl_VBlankEnabled));
	WRITE(&IRCtl_LCDStatusEnabled, sizeof(IRCtl_LCDStatusEnabled));
	WRITE(&IRCtl_TimerEnabled, sizeof(IRCtl_TimerEnabled));
	WRITE(&IRCtl_SerialEnabled, sizeof(IRCtl_SerialEnabled));
	WRITE(&IRCtl_JoypadEnabled, sizeof(IRCtl_JoypadEnabled));
	WRITE(&IRCtl_EnableRegisterByte, sizeof(IRCtl_EnableRegisterByte));
	WRITE(&IRCtl_VBlankRequested, sizeof(IRCtl_VBlankRequested));
	WRITE(&IRCtl_LCDStatusRequested, sizeof(IRCtl_LCDStatusRequested));
	WRITE(&IRCtl_TimerRequested, sizeof(IRCtl_TimerRequested));
	WRITE(&IRCtl_SerialRequested, sizeof(IRCtl_SerialRequested));
	WRITE(&IRCtl_JoypadRequested, sizeof(IRCtl_JoypadRequested));

	// MMU
	WRITE(&MMU_bCGB, sizeof(MMU_bCGB));
	WRITE(&MMU_hram[0], 128);

	size_prefix = MMU_CompressedWRAM.size();
	WRITE(&size_prefix, sizeof(size_prefix));
	WRITE(MMU_CompressedWRAM.data(), size_prefix);

	// MBC
	WRITE(&MBC_cp, sizeof(MBC_cp));
	WRITE(&MBC_bankingMode, sizeof(MBC_bankingMode));
	WRITE(&MBC_romBank, sizeof(MBC_romBank));
	WRITE(&MBC_romOffset, sizeof(MBC_romOffset));
	WRITE(&MBC_exRAMEnabled, sizeof(MBC_exRAMEnabled));
	WRITE(&MBC_extRAMBank, sizeof(MBC_extRAMBank));
	WRITE(&MBC_numExtRAMBanks, sizeof(MBC_numExtRAMBanks));
	WRITE(&MBC_rtcEnabled, sizeof(MBC_rtcEnabled));
	WRITE(&MBC_zeroSeen, sizeof(MBC_zeroSeen));
	WRITE(&MBC_latchedRTCRegister, sizeof(MBC_latchedRTCRegister));
	WRITE(&MBC_dayCtr, sizeof(MBC_dayCtr));
	WRITE(&MBC_lastLatchedTime, sizeof(MBC_lastLatchedTime));
	WRITE(&MBC_daysOverflowed, sizeof(MBC_daysOverflowed));

	// Skipping external RAM
	
	
	// CGBRegisters
	WRITE(&CGBReg_wramBank, sizeof(CGBReg_wramBank));
	WRITE(&CGBReg_wramOffset, sizeof(CGBReg_wramOffset));
	WRITE(&CGBReg_prepareSpeedSwitch, sizeof(CGBReg_prepareSpeedSwitch));
	WRITE(&CGBReg_currentSpeed, sizeof(CGBReg_currentSpeed));

	// TimerController
	WRITE(&TmrCtl_Divider, sizeof(TmrCtl_Divider));
	WRITE(&TmrCtl_Counter, sizeof(TmrCtl_Counter));
	WRITE(&TmrCtl_Modulo, sizeof(TmrCtl_Modulo));
	WRITE(&TmrCtl_Speed, sizeof(TmrCtl_Speed));
	WRITE(&TmrCtl_TACRegisterByte, sizeof(TmrCtl_TACRegisterByte));
	WRITE(&TmrCtl_Running, sizeof(TmrCtl_Running));
	WRITE(&TmrCtl_bCGB, sizeof(TmrCtl_bCGB));
	WRITE(&TmrCtl_divAcc, sizeof(TmrCtl_divAcc));
	WRITE(&TmrCtl_ctrAcc, sizeof(TmrCtl_ctrAcc));
	WRITE(&TmrCtl_tCyclesPerCtrCycle, sizeof(TmrCtl_tCyclesPerCtrCycle));

	// SerialController
	WRITE(&SerialCtl_TxStart, sizeof(SerialCtl_TxStart));
	WRITE(&SerialCtl_ShiftClockType, sizeof(SerialCtl_ShiftClockType));
	WRITE(&SerialCtl_RegisterByte, sizeof(SerialCtl_RegisterByte));

	// GPU
	WRITE(&GPU_bCGB, sizeof(GPU_bCGB));
	WRITE(&GPU_tAcc, sizeof(GPU_tAcc));
	WRITE(&GPU_vramBank, sizeof(GPU_vramBank));
	WRITE(&GPU_vramOffset, sizeof(GPU_vramOffset));
	WRITE(&GPU_dma, sizeof(GPU_dma));
	WRITE(&GPU_dmaSrc, sizeof(GPU_dmaSrc));
	WRITE(&GPU_dmaDest, sizeof(GPU_dmaDest));
	WRITE(&GPU_control, sizeof(GPU_control));
	WRITE(&GPU_positions, sizeof(GPU_positions));
	WRITE(&GPU_stat, sizeof(GPU_stat));
	WRITE(&GPU_bgMonoPalette, sizeof(MonochromePalette));
	WRITE(&GPU_sprMonoPalettes[0], sizeof(MonochromePalette) * 2);
	WRITE(&GPU_bgColourPalette, sizeof(ColourPalette));
	WRITE(&GPU_sprColourPalette, sizeof(ColourPalette));
	WRITE(&GPU_correctionMode, sizeof(GPU_correctionMode));
	WRITE(&GPU_brightness, sizeof(GPU_brightness));

	///////
	// VRAM
	size_prefix = GPU_CompressedVRAM.size();
	WRITE(&size_prefix, sizeof(size_prefix));
	WRITE(GPU_CompressedVRAM.data(), size_prefix);

	///////
	// OAM
	WRITE(&GPU_oam[0], GPU::OAMSize);

	///////
	// Sprites
	size_prefix = GPU_CompressedSprites.size();
	WRITE(&size_prefix, sizeof(size_prefix));
	WRITE(GPU_CompressedSprites.data(), size_prefix);

	return true;
}

bool RewindSnapshot::ReadFromStream(istream& input, streamsize& count)
{
#define READ(dest, size) input.read(reinterpret_cast<char*>(dest), size); if (input.good()) { count += size; } else { return false; }

	int size_prefix;

	// Z80 state

	READ(&Z80_PC, sizeof(Z80_PC));
	READ(&Z80_SP, sizeof(Z80_SP));
	READ(&Z80_bCGB, sizeof(Z80_bCGB));
	READ(&Z80_interruptMasterEnable, sizeof(Z80_interruptMasterEnable));
	READ(&Z80_isStopped, sizeof(Z80_isStopped));
	READ(&Z80_isHalted, sizeof(Z80_isHalted));
	READ(&Z80_registerFile[0], 8);

	// InterruptCtl
	READ(&IRCtl_VBlankEnabled, sizeof(IRCtl_VBlankEnabled));
	READ(&IRCtl_LCDStatusEnabled, sizeof(IRCtl_LCDStatusEnabled));
	READ(&IRCtl_TimerEnabled, sizeof(IRCtl_TimerEnabled));
	READ(&IRCtl_SerialEnabled, sizeof(IRCtl_SerialEnabled));
	READ(&IRCtl_JoypadEnabled, sizeof(IRCtl_JoypadEnabled));
	READ(&IRCtl_EnableRegisterByte, sizeof(IRCtl_EnableRegisterByte));
	READ(&IRCtl_VBlankRequested, sizeof(IRCtl_VBlankRequested));
	READ(&IRCtl_LCDStatusRequested, sizeof(IRCtl_LCDStatusRequested));
	READ(&IRCtl_TimerRequested, sizeof(IRCtl_TimerRequested));
	READ(&IRCtl_SerialRequested, sizeof(IRCtl_SerialRequested));
	READ(&IRCtl_JoypadRequested, sizeof(IRCtl_JoypadRequested));

	// MMU
	READ(&MMU_bCGB, sizeof(MMU_bCGB));
	READ(&MMU_hram[0], 128);

	READ(&size_prefix, sizeof(size_prefix));
	MMU_CompressedWRAM.resize(size_prefix);
	READ(MMU_CompressedWRAM.data(), size_prefix);

	// MBC
	READ(&MBC_cp, sizeof(MBC_cp));
	READ(&MBC_bankingMode, sizeof(MBC_bankingMode));
	READ(&MBC_romBank, sizeof(MBC_romBank));
	READ(&MBC_romOffset, sizeof(MBC_romOffset));
	READ(&MBC_exRAMEnabled, sizeof(MBC_exRAMEnabled));
	READ(&MBC_extRAMBank, sizeof(MBC_extRAMBank));
	READ(&MBC_numExtRAMBanks, sizeof(MBC_numExtRAMBanks));
	READ(&MBC_rtcEnabled, sizeof(MBC_rtcEnabled));
	READ(&MBC_zeroSeen, sizeof(MBC_zeroSeen));
	READ(&MBC_latchedRTCRegister, sizeof(MBC_latchedRTCRegister));
	READ(&MBC_dayCtr, sizeof(MBC_dayCtr));
	READ(&MBC_lastLatchedTime, sizeof(MBC_lastLatchedTime));
	READ(&MBC_daysOverflowed, sizeof(MBC_daysOverflowed));

	// Skipping external RAM


	// CGBRegisters
	READ(&CGBReg_wramBank, sizeof(CGBReg_wramBank));
	READ(&CGBReg_wramOffset, sizeof(CGBReg_wramOffset));
	READ(&CGBReg_prepareSpeedSwitch, sizeof(CGBReg_prepareSpeedSwitch));
	READ(&CGBReg_currentSpeed, sizeof(CGBReg_currentSpeed));

	// TimerController
	READ(&TmrCtl_Divider, sizeof(TmrCtl_Divider));
	READ(&TmrCtl_Counter, sizeof(TmrCtl_Counter));
	READ(&TmrCtl_Modulo, sizeof(TmrCtl_Modulo));
	READ(&TmrCtl_Speed, sizeof(TmrCtl_Speed));
	READ(&TmrCtl_TACRegisterByte, sizeof(TmrCtl_TACRegisterByte));
	READ(&TmrCtl_Running, sizeof(TmrCtl_Running));
	READ(&TmrCtl_bCGB, sizeof(TmrCtl_bCGB));
	READ(&TmrCtl_divAcc, sizeof(TmrCtl_divAcc));
	READ(&TmrCtl_ctrAcc, sizeof(TmrCtl_ctrAcc));
	READ(&TmrCtl_tCyclesPerCtrCycle, sizeof(TmrCtl_tCyclesPerCtrCycle));

	// SerialController
	READ(&SerialCtl_TxStart, sizeof(SerialCtl_TxStart));
	READ(&SerialCtl_ShiftClockType, sizeof(SerialCtl_ShiftClockType));
	READ(&SerialCtl_RegisterByte, sizeof(SerialCtl_RegisterByte));

	// GPU
	READ(&GPU_bCGB, sizeof(GPU_bCGB));
	READ(&GPU_tAcc, sizeof(GPU_tAcc));
	READ(&GPU_vramBank, sizeof(GPU_vramBank));
	READ(&GPU_vramOffset, sizeof(GPU_vramOffset));
	READ(&GPU_dma, sizeof(GPU_dma));
	READ(&GPU_dmaSrc, sizeof(GPU_dmaSrc));
	READ(&GPU_dmaDest, sizeof(GPU_dmaDest));
	READ(&GPU_control, sizeof(GPU_control));
	READ(&GPU_positions, sizeof(GPU_positions));
	READ(&GPU_stat, sizeof(GPU_stat));
	READ(&GPU_bgMonoPalette, sizeof(MonochromePalette));
	READ(&GPU_sprMonoPalettes[0], sizeof(MonochromePalette) * 2);
	READ(&GPU_bgColourPalette, sizeof(ColourPalette));
	READ(&GPU_sprColourPalette, sizeof(ColourPalette));
	READ(&GPU_correctionMode, sizeof(GPU_correctionMode));
	READ(&GPU_brightness, sizeof(GPU_brightness));

	///////
	// VRAM
	READ(&size_prefix, sizeof(size_prefix));
	GPU_CompressedVRAM.resize(size_prefix);
	READ(GPU_CompressedVRAM.data(), size_prefix);

	///////
	// OAM
	READ(&GPU_oam[0], GPU::OAMSize);

	///////
	// Sprites
	READ(&size_prefix, sizeof(size_prefix));
	GPU_CompressedSprites.resize(size_prefix);
	READ(GPU_CompressedSprites.data(), size_prefix);

	return true;
}