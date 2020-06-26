#pragma once

#include "Core/Instruction.h"
#include "Core/CartridgeReader.h"

class DecodedInstruction
{
	public:
		DecodedInstruction();
		DecodedInstruction(const DecodedInstruction& other);
		DecodedInstruction& operator=(const DecodedInstruction& other);
		DecodedInstruction(uint8_t* data, unsigned int size); // Initialize a disassembly made up of a region of data, not executable instructions.
		DecodedInstruction(uint16_t addr, Instruction inst); // Initialize from a single opcode.
		DecodedInstruction(uint16_t addr, Instruction inst, uint8_t b0); // Initialize from an opcode and 1 byte of immediate data.
		DecodedInstruction(uint16_t addr, Instruction inst, uint8_t b0, uint8_t b1); // Initialize from an opcode and 2 bytes of immediate data.
		~DecodedInstruction() noexcept;

		bool operator==(DecodedInstruction const& other) { return instruction == other.instruction; }
		bool operator!=(DecodedInstruction const& other) { return instruction != other.instruction; }
		
		std::string AsString() const { return as_string; };

		uint16_t GetAddress() const { return address; }
		int GetSize() const { return size; }
		std::string GetMnemonic();
		uint32_t GetDataAsLeftPaddedInt();

		static std::string TranslateToMnemonic(DecodedInstruction& dasm);
	private:
		void DeepCopy(const DecodedInstruction& other);

		void updateAsString();

		std::string as_string;
		std::string mnemonic;
		Instruction instruction;
		bool is_instruction;
		uint16_t address;
		int size;
		uint8_t* data; // Maximum number of bytes an instruction can consume
};
