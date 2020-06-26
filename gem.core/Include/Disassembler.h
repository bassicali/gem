#pragma once

#include <vector>
#include <map>
#include <memory>

#include "Core/Instruction.h"
#include "Core/MMU.h"
#include "DecodedInstruction.h"

enum DisassemblyChangeType
{
	AddNew,
	Remove,
	UpdateExisting,
	UpdateAll
};

class DisassemblyChange
{
	public:
		DisassemblyChange();
		DisassemblyChange(DisassemblyChangeType type, int index, DecodedInstruction inst);
		static DisassemblyChange CreateAddNew(DecodedInstruction inst, int index);
		static DisassemblyChange CreateRemove(int index);
		static DisassemblyChange CreateUpdate(DecodedInstruction inst, int index);
		static DisassemblyChange CreateUpdateAll();
		DisassemblyChangeType Type;
		int Index;
		DecodedInstruction AddOrUpdateInstruction;
		bool operator ==(DisassemblyChange const& other) {
			return Type == other.Type && Index == other.Index && AddOrUpdateInstruction == other.AddOrUpdateInstruction;
		}
		bool operator !=(DisassemblyChange const& other) { return !(*this == other); }
	private:
};

class Disassembler
{
	public:
		Disassembler();
		Disassembler(std::shared_ptr<MMU> memory);
		void DisassembleFromMMU(std::shared_ptr<MMU> mmu);
		void OnWriteByte(std::shared_ptr<MMU> memory, uint16_t address, uint8_t value);
		size_t GetLength() const { return rom_disassembly.size(); }
		DecodedInstruction& GetDisassembly(unsigned int index);
		const std::vector<DecodedInstruction>& GetDisassemblyList() const { return rom_disassembly; }
		size_t TranslateAddressToIndex(uint16_t pc);

		void RecordMemoryChanges(bool bRecord = true) { recording_changes = bRecord; }
		const std::vector<DisassemblyChange>& GetChanges() { return changes; }
		void ClearChanges() { changes.clear(); }
	private:
		void updateDisassembly(std::shared_ptr<MMU> memory, uint16_t address, Instruction& out_inst, int& out_inst_size, int& out_imm_size);
		DecodedInstruction decodeMemoryAtAddress(std::shared_ptr<MMU> memory, uint16_t address, Instruction& out_inst, int& out_inst_size, int& out_imm_size);
		bool isDesignatedDataAddress(uint16_t address);
		std::map<uint16_t, size_t> index_by_address;
		std::vector<DecodedInstruction> rom_disassembly;

		bool recording_changes;
		std::vector<DisassemblyChange> changes;
};
