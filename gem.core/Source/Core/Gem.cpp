#include <iomanip>
#include <iostream>
#include <fstream>

#include "Core/Gem.h"
#include "Logging.h"

using namespace std;

Gem::Gem()
	: bCGB(true)
	, gpu(new GPU())
	, apu(new APU())
	, mmu(new MMU())
	, joypad(new Joypad())
	, traceFile(nullptr)
	, tickCount(0)
	, frameCount(0)
	, tickAPU(true)
	, isTracing(false)
{
	cpu.SetMMU(mmu);
	mmu->SetGPU(gpu);
	mmu->SetAPU(apu);
	mmu->SetJoypad(joypad);

	gpu->SetInterruptController(mmu->GetInterruptController());
	gpu->SetMMU(mmu);

	joypad->SetInterruptController(mmu->GetInterruptController());
}

Gem::~Gem()
{
}

void Gem::Reset(bool bCGB)
{
	this->bCGB = bCGB;
	cpu.Reset(bCGB);
	gpu->Reset(bCGB);
	apu->Reset();
	mmu->Reset(bCGB);

	tickCount = 0;
	frameCount = 0;

	if (isTracing)
		EndTrace();
}

void Gem::Shutdown()
{
	mmu->Shutdown();
}

void Gem::LoadRom(const char* file)
{
	if (cart)
		cart.reset();

	cart = make_shared<CartridgeReader>();
	cart->LoadFile(file);

	if (!mmu->SetCartridge(cart))
	{
		throw exception("Failed to initialize MMU");
	}

	gpu->SetCartridge(cart);
}

void Gem::ToggleSound(bool enabled)
{
	tickAPU = enabled;
}

void Gem::TickUntilVBlank()
{
	while (Tick() == false);
}

bool Gem::Tick()
{
	/** FETCH */
	// In case inst == 0xCB the Z80 class will read the next byte on its own to finish the opcode
	uint16_t pc = cpu.GetPC();
	uint16_t op = uint16_t(mmu->ReadByte(pc));

	HandleTracing(pc, op);

	/** DECODE + EXECUTE */
	int m_op = 0;
	if (!cpu.IsIdle())
	{
		m_op = cpu.Execute(op);
	}
	else
	{
		// Without this a timer interrupt would never occur in the idle state.
		m_op = 1;
	}

	/** INTERRUPTS */
	m_op += cpu.HandleInterrupts();

	/** TIMERS */
	mmu->GetTimerController().TickTimers(m_op * 4);

	int t_mult = bCGB && mmu->GetCGBRegisters().Speed() == SpeedMode::Double
					? 2 : 4;

	/** APU */
	if (tickAPU)
	{
		// Tick the APU so it can fill its sound buffer
		apu->TickEmitters(m_op * t_mult);
	}

	/** GPU */
	// Tick the GPU's internal state with the m cycles
	LCDMode prev = gpu->GetLCDStatus().Mode;
	gpu->TickStateMachine(m_op * t_mult);
	bool vblank = gpu->GetLCDStatus().Mode != prev
					&& gpu->GetLCDStatus().Mode == LCDMode::VBlank;

	tickCount++;
	if (vblank) frameCount++;

	return vblank;
}

string Gem::StartTrace()
{
	using namespace std::chrono;
	auto id = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
	string file_name("gem-trace");
	file_name.append(to_string(id));
	file_name.append(".txt");
	traceFile = new ofstream(file_name.c_str(), std::ofstream::out);
	isTracing = true;
	return file_name;
}

void Gem::EndTrace()
{
	isTracing = false;
	delete traceFile;
	traceFile = nullptr;
}

inline void Gem::HandleTracing(uint16_t pc, uint16_t inst)
{
	if (!isTracing)
		return;

	OpCodeInfo* inst_info = nullptr;

	if (inst != 0xCB)
	{
		inst_info = &OpCodeIndex::Get()[inst];
	}
	else
	{
		uint8_t extended = mmu->ReadByte(pc + 1);
		inst = 0xCB00 | extended;
		inst_info = &OpCodeIndex::Get()[inst];
	}

	*traceFile << std::hex << std::uppercase
		<< "PC:" << setw(4) << setfill('0') << cpu.GetPC() << setfill(' ') << " (" << setw(10) << inst_info->Mnemonic << ")"
		<< " SP:" << setw(4) << setfill('0') << cpu.GetSP() << "(" << setw(10) << inst_info->Mnemonic << ")"
		<< " A:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterA())
		<< " B:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterB())
		<< " C:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterC())
		<< " D:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterD())
		<< " E:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterE())
		<< " H:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterH())
		<< " L:" << setw(2) << setfill('0') << unsigned(cpu.GetRegisterL())
		<< " Z:" << unsigned(cpu.GetZeroFlag())
		<< " N:" << unsigned(cpu.GetOperationFlag())
		<< " H:" << unsigned(cpu.GetHalfCarryFlag())
		<< " C:" << unsigned(cpu.GetCarryFlag())
		<< std::nouppercase << setfill(' ') << std::dec << std::endl;

	traceFile->flush();
}