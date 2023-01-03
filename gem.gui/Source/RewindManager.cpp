
#include "RewindManager.h"
#include <Core/Gem.h>

#include <cmath>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
}

RewindManager::RewindManager()
	: RewindManager(nullptr, 10.0)
{
	const int default_duration = 10.0;
	int count = (int)floor(default_duration * 30.0);
	buffer = RewindBuffer(count, true);
}

RewindManager::RewindManager(Gem* core, float durationSec)
	: joinedWorkingRAM(8 * 0x1000)
	, joinedExtRAM(4 * 0x2000)
	, joinedVRAM(2 * 0x2000)
	, joinedOAM(0xA0)
	, joinedSprites(40 * sizeof(SpriteData))
{
	int count = (int)floor(durationSec * 30.0);
	buffer = RewindBuffer(count, true);
}

bool RewindManager::InitVideoCodec()
{
	if (const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG))
	{
		if (videoCtx = avcodec_alloc_context3(codec))
		{
			videoCtx->bit_rate = 400000;
			/* resolution must be a multiple of two */
			videoCtx->width = GPU::LCDWidth;
			videoCtx->height = GPU::LCDHeight;
			/* frames per second */
			videoCtx->time_base = { 1, 30 };
			videoCtx->framerate = { 30, 1 };

			/* emit one intra frame every ten frames
				* check frame pict_type before passing frame
				* to encoder, if vidFrame->pict_type is AV_PICTURE_TYPE_I
				* then gop_size is ignored and the output of encoder
				* will always be I frame irrespective to gop_size
				*/
			videoCtx->gop_size = 0;
			videoCtx->max_b_frames = 1;
			videoCtx->pix_fmt = AV_PIX_FMT_RGBA;

			return true;
		}
	}
	
	return false;
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
	for (int i = 0; i < buffer.Count(); i++)
	{
		buffer[i].GPU_CompressedSprites.Free();
		buffer[i].GPU_CompressedVRAM.Free();
		buffer[i].MBC_CompressedExtRAM.Free();
		buffer[i].MMU_CompressedWRAM.Free();
		av_packet_free(&buffer[i].GPU_CompressedFramePacket);
	}

	if (videoCtx)
	{
		avcodec_free_context(&videoCtx);
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
}

bool RewindManager::StartRecording()
{
	isRecording == true;
	return true;
}

void RewindManager::RecordSnapshot()
{
	if (isPlaying)
	{
		return;
	}

	auto snapshot = GetSnapshot();
	AddToBuffer(snapshot);
}

RewindSnapshot RewindManager::GetSnapshot()
{
	RewindSnapshot snapshot;

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
	

	if (!joinedWorkingRAM.IsAllocated())
	{
		joinedWorkingRAM.Allocate();
		joinedWorkingRAM.Reserve();
	}

	for (int i = 0; i < 8; i++)
	{
		memcpy(joinedWorkingRAM.Ptr() + 0x1000 * i, mmu->wramBanks[i].Ptr(), mmu->wramBanks[i].Size());
	}

	snapshot.MMU_CompressedWRAM = DataBuffer(0x1000);
	snapshot.MMU_CompressedWRAM.Allocate();
	snapshot.MMU_CompressedWRAM.Reserve();

	CompressData(joinedWorkingRAM.Ptr(), joinedWorkingRAM.Size(), snapshot.MMU_CompressedWRAM);

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

	if (!joinedExtRAM.IsAllocated())
	{
		joinedExtRAM.Allocate();
		joinedExtRAM.Reserve();
	}

	for (int i = 0; i < 4; i++)
	{
		memcpy(joinedExtRAM.Ptr() + MBC::RAMBankSize * i, mbc.extRAMBanks[i].Ptr(), mbc.extRAMBanks[i].Size());
	}

	snapshot.MBC_CompressedExtRAM = DataBuffer(0x1000);
	snapshot.MBC_CompressedExtRAM.Allocate();
	snapshot.MBC_CompressedExtRAM.Reserve();

	CompressData(joinedExtRAM.Ptr(), joinedExtRAM.Size(), snapshot.MBC_CompressedExtRAM);

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
	if (!joinedVRAM.IsAllocated())
	{
		joinedVRAM.Allocate();
		joinedVRAM.Reserve();
	}

	memcpy(joinedVRAM.Ptr(), gpu->vram.Ptr(), gpu->vram.Size());
	CompressData(joinedVRAM.Ptr(), joinedVRAM.Size(), snapshot.GPU_CompressedVRAM);

	///////
	// OAM
	for (int i = 0; i < GPU::OAMSize; i++)
		snapshot.GPU_oam[i] = gpu->oam[i];

	///////
	// Sprites
	if (!joinedSprites.IsAllocated())
	{
		joinedSprites.Allocate();
		joinedSprites.Reserve();
	}

	for (int i = 0; i < gpu->NumSprites; i++)
	{
		memcpy(joinedSprites.Ptr() + i * sizeof(SpriteData), 
				&(gpu->sprites[i]), 
				gpu->NumSprites * sizeof(SpriteData));
	}

	CompressData(joinedSprites.Ptr(), joinedSprites.Size(), snapshot.GPU_CompressedSprites);

	AVPacket* packet = nullptr;
	EncodeVideoFrame(core->gpu->GetFrameBuffer(), packet);
	snapshot.GPU_CompressedFramePacket = packet;

	return snapshot;
}

void RewindManager::ApplyCurrentSnapshot()
{
	RewindSnapshot& snapshot = buffer[idxBuffPlay];
	ApplySnapshot(snapshot);
}

void RewindManager::ApplySnapshot(RewindSnapshot& snapshot)
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

	DecompressData(snapshot.MMU_CompressedWRAM.Ptr(), snapshot.MMU_CompressedWRAM.Count(), joinedWorkingRAM);

	for (int i = 0; i < 8; i++)
	{
		memcpy(mmu->wramBanks[i].Ptr(), joinedWorkingRAM.Ptr() + 0x1000 * i, mmu->wramBanks[i].Capacity());
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

	DecompressData(snapshot.MBC_CompressedExtRAM.Ptr(), snapshot.MBC_CompressedExtRAM.Count(), joinedExtRAM);

	for (int i = 0; i < 4; i++)
	{
		memcpy(mbc.extRAMBanks[i].Ptr(), joinedExtRAM.Ptr() + MBC::RAMBankSize * i, MBC::RAMBankSize);
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

	DecompressData(snapshot.GPU_CompressedVRAM.Ptr(), snapshot.GPU_CompressedVRAM.Count(), joinedVRAM);
	memcpy(gpu->vram.Ptr(), joinedVRAM.Ptr(), GPU::VRAMSize);

	DecompressData(snapshot.GPU_CompressedSprites.Ptr(), snapshot.GPU_CompressedSprites.Count(), joinedSprites);
	for (int i = 0; i < GPU::NumSprites; i++)
	{
		memcpy(&(gpu->sprites[i]), 
				joinedSprites.Ptr() + i * sizeof(SpriteData),
				GPU::NumSprites * sizeof(SpriteData));
	}

	// Video frame is decoded in 
}

void RewindManager::StopRecording()
{
	isRecording = false;
}

void RewindManager::ClearBuffer()
{
	idxBuffStart = idxBuffEnd = 0;
}

bool RewindManager::StartPlayback()
{
	if (isRecording)
	{
		return false;
	}

	savePoint = GetSnapshot();
	EncodeVideoFrame(core->gpu->GetFrameBuffer(), savePointPacket);

	idxBuffPlay = 0;
	isPlaying = true;
}

void RewindManager::GetCurrentPlaybackFrame(ColourBuffer& framebuffer)
{
	if (idxBuffEnd == idxBuffStart == 0)
	{
		return;
	}

	int len = idxBuffEnd > idxBuffStart
			  ? idxBuffEnd - idxBuffStart + 1
			  : idxBuffStart - idxBuffEnd + 1;

	if (idxBuffPlay < len)
	{
		RewindSnapshot& snapshot = buffer[idxBuffPlay];
		DecodeVideoFrame(snapshot.GPU_CompressedFramePacket, framebuffer);
		idxBuffPlay++;
	}
}

bool RewindManager::StopPlayback(bool continueFromStart)
{
	if (isRecording || !isPlaying)
	{
		return false;
	}

	if (continueFromStart)
	{
		ApplySnapshot(savePoint);
		DecodeVideoFrame(savePoint.GPU_CompressedFramePacket, core->gpu->frameBuffer);
	}

	idxBuffPlay = 0;
	isPlaying = false;
}

void RewindManager::SetBufferSize(float durationSeconds)
{
	int num_snapshots = (int)floor(durationSeconds / 30.0f);
	buffer.SetCapacity(num_snapshots);
}

bool RewindManager::AddToBuffer(const RewindSnapshot& snapshot)
{
	int insert_idx;

	if (idxBuffStart == idxBuffEnd == 0)
	{
		insert_idx = 0;
		idxBuffEnd++;
	}
	else
	{
		idxBuffEnd = (idxBuffEnd + 1) % buffer.Capacity();
		if (idxBuffEnd == idxBuffStart)
		{
			RewindSnapshot& discard = buffer[idxBuffStart];

			// circled around
			idxBuffStart = (idxBuffStart + 1) % buffer.Capacity();

			discard.GPU_CompressedSprites.Free();
			discard.GPU_CompressedVRAM.Free();
			discard.MBC_CompressedExtRAM.Free();
			discard.MMU_CompressedWRAM.Free();

			av_packet_free(&discard.GPU_CompressedFramePacket);
		}
		insert_idx = idxBuffEnd;
	}

	buffer[insert_idx] = snapshot;

	return false;
}

bool RewindManager::CompressData(const uint8_t* uncompressed_data, int len, DArray<uint8_t>& output_buffer)
{
	bool successful = true;

	if (!compressor)
	{
		successful = CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor);
	}
	
	if (successful)
	{
		bool retry = false;
		SIZE_T required_buffer_size;

		successful = Compress(compressor, (LPCVOID)uncompressed_data, (SIZE_T)len, output_buffer.Ptr(), output_buffer.Capacity(), &required_buffer_size);

		if (!successful)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				output_buffer.SetCapacity(required_buffer_size);
				retry = true;
			}
		}
		else
		{
			output_buffer.SetCount(required_buffer_size);
		}

		if (retry)
		{
			successful = Compress(compressor, (LPCVOID)uncompressed_data, (SIZE_T)len, output_buffer.Ptr(), output_buffer.Capacity(), &required_buffer_size);
			
			if (successful)
			{
				output_buffer.SetCount(required_buffer_size);
			}
		}
	}

	return successful;
}

bool RewindManager::DecompressData(const uint8_t* compressed_data, int len, DataBuffer& output_buffer)
{
	bool successful = false;

	if (!decompressor)
	{
		successful = CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor);
	}

	if (successful)
	{
		bool retry = false;
		SIZE_T required_buffer_size;

		successful = Decompress(decompressor, (LPCVOID)compressed_data, (SIZE_T)len, output_buffer.Ptr(), output_buffer.Capacity(), &required_buffer_size);

		if (!successful)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				output_buffer.SetCapacity(required_buffer_size);
				retry = true;
			}
		}

		if (retry)
		{
			successful = Decompress(decompressor, (LPCVOID)compressed_data, (SIZE_T)len, output_buffer.Ptr(), output_buffer.Capacity(), &required_buffer_size);
		}
	}

	return successful;
}

bool RewindManager::EncodeVideoFrame(const ColourBuffer& framebuffer, AVPacket*& output_packet)
{
	if (!vidFrame)
	{
		vidFrame = av_frame_alloc();
	}

	if (vidFrame)
	{
		vidFrame->format = videoCtx->pix_fmt;
		vidFrame->width = videoCtx->width;
		vidFrame->height = videoCtx->height;

		if (av_frame_make_writable(vidFrame) == 0)
		{
			for (int i = 0; i < framebuffer.Width; i++)
			{
				for (int j = 0; j < framebuffer.Height; j++)
				{
					GemColour px = framebuffer.GetPixel(i, j);
					vidFrame->data[0][i * vidFrame->linesize[0] + j] = px.Red;
					vidFrame->data[1][i * vidFrame->linesize[0] + j] = px.Green;
					vidFrame->data[2][i * vidFrame->linesize[0] + j] = px.Blue;
					vidFrame->data[3][i * vidFrame->linesize[0] + j] = 255;
				}
			}

			static long frame_timestamp = 0;
			vidFrame->pts = frame_timestamp++;

			if (avcodec_send_frame(videoCtx, vidFrame) == 0)
			{
				bool failed = false;
				int ret = 0;
				int count = 0;

				if (AVPacket* packet = av_packet_alloc())
				{
					while (ret >= 0) 
					{
						ret = avcodec_receive_packet(videoCtx, packet);
						failed = ret < 0;

						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
						{
							break;
						}

						output_packet = packet;
					}

					if (!failed)
					{
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

bool RewindManager::DecodeVideoFrame(const AVPacket* packet, ColourBuffer& output_buffer)
{
	if (avcodec_send_packet(videoCtx, packet) == 0)
	{
		bool failed = false;
		int ret = 0;
		int count = 0;

		if (!vidFrame)
		{
			vidFrame = av_frame_alloc();
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_frame(videoCtx, vidFrame);
			failed = ret < 0;

			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
			{
				break;
			}

			for (int i = 0; i < GPU::LCDWidth; i++)
			{
				for (int j = 0; j < GPU::LCDHeight; j++)
				{
					GemColour px;
					px.Red = vidFrame->data[0][i * vidFrame->linesize[0] + j];
					px.Green = vidFrame->data[1][i * vidFrame->linesize[0] + j];
					px.Blue = vidFrame->data[2][i * vidFrame->linesize[0] + j];
					px.Alpha = 255;
					output_buffer.SetPixel(i, j, px);
				}
			}
		}

		if (!failed)
		{
			return true;
		}
	}

	return true;
}

int RewindSnapshot::Size()
{
	int size =
		sizeof(RewindSnapshot) 
		- (5*sizeof(DataBuffer))
		+ MMU_CompressedWRAM.Size()
		+ MBC_CompressedExtRAM.Size()
		+ GPU_CompressedVRAM.Size()
		+ GPU_CompressedSprites.Size();
	
	return size;
}
