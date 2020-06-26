#include <iomanip>
#include <iostream>
#include <fstream>

#include "Core/Gem.h"
#include "Logging.h"

using namespace std;

Gem::Gem() : 
	bCGB(true),
	gpu(new GPU()),
	apu(new APU()),
	mmu(new MMU()),
	joypad(new Joypad()),
	traceFile(nullptr),
	tickCount(0),
	frameCount(0),
	dasmEnabled(false),
	tickAPU(true),
	tracing(false)
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

	if (tracing)
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

void Gem::EnableDisassembly()
{
	using namespace std::placeholders;

	dasm.DisassembleFromMMU(mmu);
	dasmEnabled = true;
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
	int m_irq = cpu.HandleInterrupts();

	/** TIMERS */
	mmu->GetTimerController().TickTimers((m_op + m_irq) * 4);

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
	tracing = true;
	return file_name;
}

void Gem::EndTrace()
{
	tracing = false;
	delete traceFile;
	traceFile = nullptr;
}

inline void Gem::HandleTracing(uint16_t pc, uint16_t inst)
{
	if (!tracing)
		return;

	const char* mnemonic;
	if (inst != 0xCB)
	{
		mnemonic = Z80::InstructionNameLookup[static_cast<Instruction>(inst)];
	}
	else
	{
		uint8_t extended = mmu->ReadByte(pc + 1);
		uint16_t inst = 0xCB00 | extended;
		mnemonic = Z80::InstructionNameLookup[static_cast<Instruction>(inst)];
	}

	*traceFile << std::hex << std::uppercase
		<< "PC:" << setw(4) << setfill('0') << cpu.GetPC() << setfill(' ') << " (" << setw(10) << mnemonic << ")"
		<< " SP:" << setw(4) << setfill('0') << cpu.GetSP() << "(" << setw(10) << mnemonic << ")"
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

void Gem::TimingTest()
{
	int TickTable[256] = {
		/*  0, 1, 2, 3, 4, 5,  6, 7,        8, 9, A, B, C, D,  E, F*/
		4, 12,  8,  8,  4,  4,  8,  4,     20,  8,  8, 8,  4,  4, 8,  4,  //0
		4, 12,  8,  8,  4,  4,  8,  4,     12,  8,  8, 8,  4,  4, 8,  4,  //1
		8, 12,  8,  8,  4,  4,  8,  4,      8,  8,  8, 8,  4,  4, 8,  4,  //2
		8, 12,  8,  8, 12, 12, 12,  4,      8,  8,  8, 8,  4,  4, 8,  4,  //3

		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //4
		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //5
		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //6
		8,  8,  8,  8,  8,  8,  4,  8,      4,  4,  4, 4,  4,  4, 8,  4,  //7

		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //8
		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //9
		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //A
		4,  4,  4,  4,  4,  4,  8,  4,      4,  4,  4, 4,  4,  4, 8,  4,  //B

		8, 12, 12, 16, 12, 16,  8, 16,      8, 16, 12, 0, 12, 24, 8, 16,  //C
		8, 12, 12,  4, 12, 16,  8, 16,      8, 16, 12, 4, 12,  4, 8, 16,  //D
		12, 12,  8,  4,  4, 16,  8, 16,     16,  4, 16, 4,  4,  4, 8, 16,  //E
		12, 12,  8,  4,  4, 16,  8, 16,     12,  8, 16, 4,  0,  4, 8, 16   //F
	};

	int CBTickTable[256] = {
		/*  0, 1, 2, 3, 4, 5,  6, 7,        8, 9, A, B, C, D,  E, F*/
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //0
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //1
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //2
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //3

		8, 8, 8, 8, 8, 8, 12, 8,        8, 8, 8, 8, 8, 8, 12, 8,  //4
		8, 8, 8, 8, 8, 8, 12, 8,        8, 8, 8, 8, 8, 8, 12, 8,  //5
		8, 8, 8, 8, 8, 8, 12, 8,        8, 8, 8, 8, 8, 8, 12, 8,  //6
		8, 8, 8, 8, 8, 8, 12, 8,        8, 8, 8, 8, 8, 8, 12, 8,  //7

		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //8
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //9
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //A
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //B

		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //C
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //D
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8,  //E
		8, 8, 8, 8, 8, 8, 16, 8,        8, 8, 8, 8, 8, 8, 16, 8   //F
	};

	cout << left << setw(10) << setfill(' ') << "Inst";
	cout << left << setw(10) << setfill(' ') << "Expected";
	cout << left << setw(10) << setfill(' ') << "Actual" << endl;
	for (auto& pair : Z80::InstructionNameLookup)
	{
		int opcode = static_cast<int>(pair.first);

		int expected_cycles = 0;
		if ((opcode & 0xCB00) == 0xCB00)
			expected_cycles = CBTickTable[opcode & 0xFF] / 4;
		else
			expected_cycles = TickTable[opcode] / 4;

		int mcycles = cpu.Execute(opcode);

		cpu.Reset(true);

		if (mcycles != expected_cycles)
		{
			cout << left << setw(10) << setfill(' ') << pair.second;
			cout << left << setw(10) << setfill(' ') << expected_cycles;
			cout << left << setw(10) << setfill(' ') << mcycles << endl;
		}
	}

	int x = 0;
}
