#include "Disassembler.h"

using namespace std;

Disassembler::Disassembler() :
	rom_disassembly()
{
	
}

Disassembler::Disassembler(shared_ptr<MMU> memory) :
	rom_disassembly(1024)
{
	DisassembleFromMMU(memory);
}

DecodedInstruction& Disassembler::GetDisassembly(unsigned int index)
{
	if (index >= rom_disassembly.size())
	{
		throw exception("Index too large for disassembly.");
	}

	return rom_disassembly[index];
}

size_t Disassembler::TranslateAddressToIndex(uint16_t pc)
{
	if (index_by_address.find(pc) != index_by_address.end())
	{
		return index_by_address[pc];
	}

	return -1;
}

void Disassembler::DisassembleFromMMU(shared_ptr<MMU> memory)
{
	uint16_t addr;
	int stride = 0;
	Instruction inst;
	int inst_size;
	int imm_size;

	for (addr = 0; addr < 0x8000; addr += stride)
	{
		updateDisassembly(memory, addr, inst, inst_size, imm_size);
		stride = inst_size + imm_size;
	}

	// Skip VRAM

	if (memory->GetCartridgeReader()->Properties().HasExtRam)
	{
		for (addr = 0xA000; addr < 0xC000; addr += stride)
		{
			updateDisassembly(memory, addr, inst, inst_size, imm_size);
			stride = inst_size + imm_size;
		}
	}

	for (addr = 0xC000; addr < 0xFE00; addr += stride)
	{
		updateDisassembly(memory, addr, inst, inst_size, imm_size);
		stride = inst_size + imm_size;
	}

	// Skip everything after FDFFh; I don't think games ever execute from that region.
}

void Disassembler::OnWriteByte(shared_ptr<MMU> memory, uint16_t address, uint8_t value)
{
	auto it = index_by_address.find(address);
	if (it != index_by_address.end())
	{
		size_t index = it->second;

		uint16_t starting_address = rom_disassembly[index].GetAddress();

		Instruction inst;
		int inst_size;
		int imm_size;
		DecodedInstruction updated_inst = decodeMemoryAtAddress(memory, starting_address, inst, inst_size, imm_size);

		rom_disassembly[index] = updated_inst;

		if (recording_changes)
		{
			if (changes.size() > 20000)
			{
				changes.clear();
				changes.push_back(DisassemblyChange::CreateUpdateAll());
			}
			else if (changes.size() != 1 || changes[0].Type != DisassemblyChangeType::UpdateAll)
			{
				changes.push_back(DisassemblyChange::CreateUpdate(updated_inst, index));
			}
		}
	}
}

void Disassembler::updateDisassembly(shared_ptr<MMU> memory, uint16_t address, Instruction& out_inst, int& out_inst_size, int& out_imm_size)
{
	DecodedInstruction inst = decodeMemoryAtAddress(memory, address, out_inst, out_inst_size, out_imm_size);
	
	if (out_imm_size == 0)
	{
		rom_disassembly.push_back(inst);
		index_by_address[address] = rom_disassembly.size() - 1;
	}
	else if (out_imm_size == 1)
	{
		rom_disassembly.push_back(inst);
		index_by_address[address] = rom_disassembly.size() - 1;
		index_by_address[address + 1] = rom_disassembly.size() - 1;
	}
	else if (out_imm_size == 2)
	{
		rom_disassembly.push_back(inst);
		index_by_address[address] = rom_disassembly.size() - 1;
		index_by_address[address + 1] = rom_disassembly.size() - 1;
		index_by_address[address + 2] = rom_disassembly.size() - 1;
	}
}

DecodedInstruction Disassembler::decodeMemoryAtAddress(shared_ptr<MMU> memory, uint16_t address, Instruction& out_inst, int& out_inst_size, int& out_imm_size)
{
	uint16_t inst_bytes;
	uint8_t b0 = memory->ReadByte(address);
	uint8_t b1;
	if (b0 == 0xCB)
	{
		b1 = memory->ReadByte(address + 1);
		inst_bytes = 0xCB00 | b1;
		out_inst_size = 2;
	}
	else
	{
		inst_bytes = uint16_t(b0);
		out_inst_size = 1;
	}

	out_inst = static_cast<Instruction>(inst_bytes);
	out_imm_size = GetInstructionImmSize(out_inst);

	if (out_imm_size == 0)
	{
		return DecodedInstruction(address, out_inst);
	}
	else if (out_imm_size == 1)
	{
		return DecodedInstruction(address, out_inst, memory->ReadByte(address + out_inst_size));
	}
	else if (out_imm_size == 2)
	{
		return DecodedInstruction(address, out_inst, memory->ReadByte(address + out_inst_size), memory->ReadByte(address + out_inst_size + 1));
	}

	return DecodedInstruction();
}

bool Disassembler::isDesignatedDataAddress(uint16_t address)
{
	bool is_rom_header =	address >= 0x0104 && address <= 0x014F;
	bool is_vram =			address >= 0x8000 && address <= 0x9FFF;
	bool is_oam =			address >= 0xFE00 && address <= 0xFE9F;
	bool is_io =			address >= 0xFF00 && address <= 0xFF7F;

	return is_rom_header || is_vram || is_oam || is_io;
}

DisassemblyChange::DisassemblyChange() :
	Type(AddNew), 
	Index(0),
	AddOrUpdateInstruction()
{
}

DisassemblyChange::DisassemblyChange(DisassemblyChangeType type, int index, DecodedInstruction inst) :
	Type(type),
	Index(index)
{
	AddOrUpdateInstruction = inst;
}

DisassemblyChange DisassemblyChange::CreateAddNew(DecodedInstruction inst, int index)
{
	return DisassemblyChange(DisassemblyChangeType::AddNew, index, inst);
}

DisassemblyChange DisassemblyChange::CreateRemove(int index)
{
	return DisassemblyChange(DisassemblyChangeType::Remove, index, DecodedInstruction());
}

DisassemblyChange DisassemblyChange::CreateUpdate(DecodedInstruction inst, int index)
{
	return DisassemblyChange(DisassemblyChangeType::UpdateExisting, index, DecodedInstruction());
}

DisassemblyChange DisassemblyChange::CreateUpdateAll()
{
	return DisassemblyChange(DisassemblyChangeType::UpdateAll, 0, DecodedInstruction());
}