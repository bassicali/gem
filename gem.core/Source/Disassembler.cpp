
#include <list>
#include <cassert>

#include "Disassembler.h"
#include "Util.h"
#include "Core/Instruction.h"
#include "Core/Z80.h"
#include "Core/MMU.h"

using namespace std;
using namespace GemUtil;

DisassemblyEntry DisassemblyEntry::Unknown = DisassemblyEntry();

string DisassemblyEntry::GetMnemonic() const
{
	OpCodeInfo op;
	if (OpCodeIndex::Get().Lookup(OpCode, op))
	{
		string mnemonic = op.Mnemonic;
		char imm_str[5];
		if (NumImmediates == 0)
		{
			return mnemonic;
		}
		else if (NumImmediates == 1)
		{
			sprintf_s(imm_str, "%02X", Imm0);
			StringReplace(mnemonic, "{imm0}", imm_str);
		}
		else if (NumImmediates == 2)
		{
			sprintf_s(imm_str, "%02X", Imm0);
			StringReplace(mnemonic, "{imm0}", imm_str);

			sprintf_s(imm_str, "%02X", Imm1);
			StringReplace(mnemonic, "{imm1}", imm_str);
		}

		return StringLower(mnemonic);
	}

	return string();
}

wstring DisassemblyEntry::GetMnemonicW() const
{
	return std::wstring();
}


//bool Disassembler::isDesignatedDataAddress(uint16_t address)
//{
//	bool is_rom_header =	address >= 0x0104 && address <= 0x014F;
//	bool is_vram =			address >= 0x8000 && address <= 0x9FFF;
//	bool is_oam =			address >= 0xFE00 && address <= 0xFE9F;
//	bool is_io =			address >= 0xFF00 && address <= 0xFF7F;
//
//	return is_rom_header || is_vram || is_oam || is_io;
//}
//


Disassembler::Disassembler()
{
}

// Is addr an opcode or is it an immediate value?
bool IsImmediateValue(uint16_t addr, MMU& memory)
{
	auto& index = OpCodeIndex::Get();

	uint16_t b2 = memory.ReadByte(addr - 2);
	if (index.Contains(b2) && index.GetImmSize(b2) == 2)
	{
		return true;
	}

	uint16_t b1 = memory.ReadByte(addr - 1);
	if (index.Contains(b1) && index.GetImmSize(b1) == 1)
	{
		return true;
	}

	uint16_t b0 = memory.ReadByte(addr);
	return index.Contains(b0);
}

bool IsBetween(int x, int a, int b)
{
	if (b > a)
		return x >= a && x < b;

	return x >= b && x < a;
}

bool CrossesMemMapBoundary(uint16_t a, uint16_t b)
{
	if (a <= 0x7FFF && b >= 0x8000)
		return true;
	else if (a <= 0x9FFF && b >= 0xA000)
		return true;
	else if (a <= 0xCFFF && b >= 0xD000)
		return true;
	else if (a <= 0xDFFF && b >= 0xE000)
		return true;

	return false;
}

bool ReadByte(uint16_t addr, uint16_t prev_addr, MMU& memory, bool stop_at_boundary, uint8_t& out_byte)
{
	if (addr < 0 || addr > 0xFFFF)
		return false;

	if (stop_at_boundary && CrossesMemMapBoundary(prev_addr, addr))
		return false;

	out_byte = memory.ReadByte(addr);
	return true;
}

/*static*/
bool Disassembler::FindAnchor(uint16_t start_addr, MMU& memory, int max_look_behind, uint16_t& out_anchor)
{
	uint16_t curr_addr = 0;
	uint16_t prev_addr = 0;
	uint16_t anchor = start_addr;

	int offset = 0;
	while (offset < max_look_behind)
	{
		curr_addr = start_addr - offset;

		if (prev_addr != 0 && CrossesMemMapBoundary(prev_addr, curr_addr))
		{
			anchor = prev_addr;
			break;
		}

		uint8_t b;
		if (prev_addr != 0 && !ReadByte(curr_addr, prev_addr, memory, false, b))
			break;

		if (prev_addr == 0)
			prev_addr = curr_addr;

		if (!OpCodeIndex::Get().Contains(b) && b != 0xCB)
		{
			anchor = curr_addr + 1;
			break;
		}
		else if (CrossesMemMapBoundary(curr_addr - 1, curr_addr))
		{
			anchor = curr_addr - 1;
			break;
		}

		// TODO: call/jmp addrs can also be used as anchors

		offset++;
	}

	if (anchor != start_addr)
	{
		out_anchor = anchor;
		return true;
	}

	return false;
}

/*static*/
void Disassembler::Decode(uint16_t addr, int count, bool look_behind_for_anchor, MMU& memory, DisassemblyChain& out_list, DisassemblyChunk*& out_chunk_ptr, int& out_chunk_index)
{
	uint16_t anchor;
	if (look_behind_for_anchor && FindAnchor(addr, memory, 30, anchor))
	{
		addr = anchor;
	}

#define READ_OR_STOP(addr,b) if (first_addr != 0 && !ReadByte(addr, first_addr, memory, true, b)) { break; }

	bool stop = false;
	uint16_t opcode;
	uint16_t first_addr = 0;
	uint8_t b0, b1, b2;

	int chunk_idx;
	if (!out_chunk_ptr)
	{
		out_chunk_ptr = out_list.GetOrCreateChunk(addr);
		out_chunk_index = 0;
	}
	else
	{
		auto& last = out_chunk_ptr->Entries[out_chunk_ptr->Entries.size() - 1];
		assert(out_chunk_ptr->Entries.size() > 0);
		assert(addr == (last.Address + last.GetSize() + 1));

		// Decode() is being called to extend an existing chunk
		out_chunk_index++;
	}

	int start_size = out_chunk_ptr->Entries.size();
	int added = 0;
	bool reached_end = false;

	while (!stop && !reached_end && added < count)
	{
		if (addr > 0x101 && addr < 0x150)
		{
			// Cartridge header here
			break;
		}

		if (!ReadByte(addr, first_addr, memory, /*stop_at_boundary=*/ added > 0, b0))
		{
			break;
		}

		if (added == 0)
			first_addr = addr;

		if (b0 == 0xCB)
		{
			READ_OR_STOP(addr + 1, b1);
			opcode = 0xCB00 | b1;
		}
		else
		{
			opcode = b0;
		}

		assert(OpCodeIndex::Get().Contains(opcode));

		int imm = OpCodeIndex::Get().GetImmSize(opcode);
		if (imm == 0)
		{
			out_chunk_ptr->Entries.push_back(DisassemblyEntry(static_cast<OpCode>(opcode), addr));
		}
		else if (imm == 1)
		{
			READ_OR_STOP(addr + 1, b1);
			out_chunk_ptr->Entries.push_back(DisassemblyEntry(static_cast<OpCode>(opcode), b1, addr));
		}
		else
		{
			READ_OR_STOP(addr + 1, b1);
			READ_OR_STOP(addr + 2, b2);
			out_chunk_ptr->Entries.push_back(DisassemblyEntry(static_cast<OpCode>(opcode), b1, b2, addr));
		}

		out_list.TotalLen++;
		added++;

		// IMPORTANT TODO: merge this chunk with the next one if it's about to overlap

		uint16_t prev = addr;
		addr += imm + 1;
		addr += b0 == 0xCB ? 1 : 0;
		reached_end = addr < prev;
	}

#undef READ_OR_STOP

	return;
}

void Disassembler::DecodeMore(int count, MMU& memory, DisassemblyChain& out_list, DisassemblyChunk* chunk)
{
	uint16_t start_addr = chunk->Entries[chunk->Entries.size() - 1].Address;
	int index;
	Decode(start_addr, count, false, memory, out_list, chunk, index);
}

void Disassembler::UpdateChain(DisassemblyChain& chain, uint16_t new_addr, int& curr_chunk_index, DisassemblyChunk*& chunk)
{
	if (curr_chunk_index < (chunk->Entries.size() - 1)
		&& chunk->Entries[curr_chunk_index + 1].Address == new_addr)
	{
		// TODO: if reaching the end of the chunk, decode more

		curr_chunk_index++;

		return;
	}

	assert(0);
}

// Maintains a sorted linked list of disassembled chunks of assembly
DisassemblyChunk* DisassemblyChain::GetOrCreateChunk(uint16_t addr)
{
	// Find a hole in the disassembled address ranges where this new chunk can be inserted
	DisassemblyChunk* chunk_ptr = nullptr;
	ChunkChain::iterator it = Chunks.begin();
	for (auto& chunk : Chunks)
	{
		if (addr >= chunk.StartAddr() && addr <= (chunk.EndAddr() + chunk.Last().GetSize()))
		{
			chunk_ptr = &chunk;
			break;
		}
		else if (addr < chunk.StartAddr())
		{
			break;
		}

		it++;
	}

	if (chunk_ptr)
	{
		return chunk_ptr;
	}

	auto inserted = Chunks.insert(it, DisassemblyChunk());
	return &(*inserted);
}

DisassemblyChunk* DisassemblyChain::InsertAfter(DisassemblyChunk* chunk)
{
	ChunkChain::iterator it = Chunks.begin();

	for (; it != Chunks.end(); it++)
	{
		if (chunk == &(*it))
		{
			break;
		}
	}

	auto inserted = Chunks.insert(it, DisassemblyChunk());
	return &(*inserted);
}
