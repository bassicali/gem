#pragma once

#include <vector>
#include <map>
#include <memory>

#include "Core/Instruction.h"

class Z80;
class MMU;

struct DisassemblyEntry
{
	DisassemblyEntry()
		: OpCode(OpCode::NOP)
		, Imm0(0)
		, Imm1(0)
		, NumImmediates(0)
		, Breakpoint(false)
		, Address(Address)
	{
	}

	DisassemblyEntry(OpCode opcode, uint16_t Address)
		: OpCode(opcode)
		, Imm0(0)
		, Imm1(0)
		, NumImmediates(0)
		, Breakpoint(false)
		, Address(Address)
	{
	}

	DisassemblyEntry(OpCode opcode, uint8_t imm0, uint16_t Address)
		: OpCode(opcode)
		, Imm0(imm0)
		, Imm1(0)
		, NumImmediates(1)
		, Breakpoint(false)
		, Address(Address)
	{
	}

	DisassemblyEntry(OpCode opcode, uint8_t imm0, uint8_t imm1, uint16_t Address)
		: OpCode(opcode)
		, Imm0(imm0)
		, Imm1(imm1)
		, NumImmediates(2)
		, Breakpoint(false)
		, Address(Address)
	{
	}

	std::string GetMnemonic() const;
	std::wstring GetMnemonicW() const;
	int GetSize() const { return (uint16_t(OpCode) & 0xFF00) == 0xCB00; }

	static DisassemblyEntry Unknown;

	OpCode OpCode;
	uint8_t Imm0;
	uint8_t Imm1;
	int NumImmediates;
	bool Breakpoint;
	uint16_t Address;
};

struct DisassemblyChunk
{
	std::vector<DisassemblyEntry> Entries;

	DisassemblyEntry& Last() { return Entries[Entries.size() - 1]; }
	int StartAddr() const { return Entries.size() > 0 ? Entries[0].Address : -1; }
	int EndAddr() const { return Entries.size() > 0 ? Entries[Entries.size() - 1].Address : -1; }
	int Size() const { return Entries.size(); }
};

typedef std::list<DisassemblyChunk> ChunkChain;

struct DisassemblyChain
{
	ChunkChain Chunks;
	int TotalLen;

	DisassemblyChain()
		: TotalLen(0)
	{
	}

	DisassemblyChunk* GetOrCreateChunk(uint16_t addr);
	DisassemblyChunk* InsertAfter(DisassemblyChunk* chunk);
};

class Disassembler
{
public:
	Disassembler();
	static void Decode(uint16_t addr, int count, bool look_behind_for_anchor, MMU& memory, DisassemblyChain& out_list, DisassemblyChunk*& out_chunk_ptr, int& out_chunk_index);
	static void DecodeMore(int count, MMU& mmu, DisassemblyChain& out_list, DisassemblyChunk* chunk_ptr);
	static void UpdateChain(DisassemblyChain& chain, uint16_t new_addr, int& curr_chunk_index, DisassemblyChunk*& chunk);
private:
	static bool FindAnchor(uint16_t curr_pc, MMU& memory, int max_look_behind, uint16_t& out_anchor);
};
