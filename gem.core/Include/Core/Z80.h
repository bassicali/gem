#pragma once

#include <fstream>
#include <cstdint>
#include <functional>
#include <map>

#include "Core/MMU.h"
#include "Instruction.h"

#define ZERO_MASK		0x80
#define OPERATION_MASK	0x40
#define HALF_CARRY_MASK 0x20
#define CARRY_MASK		0x10

#define BC_MASK 0x0
#define DE_MASK 0x1
#define HL_MASK 0x2
#define AF_MASK 0x3
#define SP_MASK 0x3

/*
F register
0x80 = Zero flag. Set if last op resulted in a 0.
0x40 = Operation flag. Set if the last op was a subtraction.
0x20 = Half carry flag. Set if the last op's result's lower half overflowed past 15.
0x10 = Carry flag. Set if last op's result overflowed past 255 (for additions) or below 0 (for subtractions).
*/

class Z80
{
	public: 
		Z80();
		void Reset(bool bCGB);
		int Execute(uint16_t inst);

		void SetMMU(std::shared_ptr<MMU> ptr);

		bool IsInterruptsEnabled() const { return interruptMasterEnable; }
		int HandleInterrupts();
		bool IsIdle() const { return isHalted || isStopped; }

		uint16_t GetPC() const { return PC; }
		void AdvancePC() { PC++; }

		uint16_t GetSP() const { return SP; }
		int GetCallsOnStack() const { return calls_on_stack; }

		// TODO: store these values as booleans
		uint8_t GetCarryFlag()		const { return (registerFile[F] & CARRY_MASK)		>> 4; }
		uint8_t GetHalfCarryFlag()	const { return (registerFile[F] & HALF_CARRY_MASK)	>> 5; }
		uint8_t GetZeroFlag()		const { return (registerFile[F] & ZERO_MASK)		>> 7; }
		uint8_t GetOperationFlag()	const { return (registerFile[F] & OPERATION_MASK)	>> 6; }

		uint8_t GetRegisterA() const { return registerFile[7]; }
		uint8_t GetRegisterB() const { return registerFile[0]; }
		uint8_t GetRegisterC() const { return registerFile[1]; }
		uint8_t GetRegisterD() const { return registerFile[2]; }
		uint8_t GetRegisterE() const { return registerFile[3]; }
		uint8_t GetRegisterH() const { return registerFile[4]; }
		uint8_t GetRegisterL() const { return registerFile[5]; }
		uint8_t GetRegisterF() const { return registerFile[6]; }

		static std::map<Instruction, const char*> InstructionNameLookup;

	private:
		std::shared_ptr<MMU> mmu;
		std::shared_ptr<InterruptController> interrupts;

		static const int A = 7;
		static const int B = 0;
		static const int C = 1;
		static const int D = 2;
		static const int E = 3;
		static const int H = 4;
		static const int L = 5;
		static const int F = 6;

		// CPU components
		uint8_t registerFile[8]; // register file
		uint16_t PC; // program counter
		uint16_t SP; // stack pointer
		bool bCGB;
		int calls_on_stack = 0;
		int push_pop_balance = 0;

		// Indicates that PC should not be advanced incase of instructions that specifically set the PC
		bool disablePCAdvance;

		bool interruptMasterEnable;
		bool isStopped;
		bool isHalted;

		// Memory helpers
		uint8_t ReadByteByRegPair(uint8_t R0, uint8_t R1);
		inline void WriteByteByRegPair(uint8_t R0, uint8_t R1, uint8_t value);
		
		// Arithmetic helpers
		inline uint8_t AddBytes(uint8_t b1, uint8_t b2, bool with_carry = false);

		// Bit manipulation and flag reg helpers
		uint8_t MaskAndShiftRight(uint8_t val, uint8_t mask, int shift);
		uint8_t MaskAndShiftLeft(uint8_t val, uint8_t mask, int shift);

		uint16_t ConcatRegisterPair(uint8_t R0, uint8_t R1);
		void WriteWordToRegisterPair(uint8_t R0, uint8_t R1, uint16_t value);

		void SetCarryFlag(uint8_t v);
		void SetCarryFlag(bool v);
		void ClearCarryFlag() { SetCarryFlag((uint8_t) 0); }

		void SetHalfCarryFlag(uint8_t v);
		void SetHalfCarryFlag(bool bSet);
		void ClearHalfCarryFlag() { SetHalfCarryFlag(uint8_t(0)); }
		void CalculateHalfCarry(uint8_t opl, uint8_t opr, uint8_t res);

		void SetZeroFlag(uint8_t v);
		void SetZeroFlag(bool bSet);

		void SetOperationFlag(uint8_t v);
		void SetOperationFlag(bool bSet);

		void TestBit(int b, uint8_t val);
		uint8_t ClearBit(int b, uint8_t value);
		uint8_t SetBit(int b, uint8_t value);
		void SetRegisterBit(int b, int r);
		void ClearRegisterBit(int b, int r);

		void RotateRegister(int r, Direction d, RotateCarryBehavior carry_behavior);
		uint8_t RotateByte(uint8_t value, Direction d, RotateCarryBehavior carry_behavior);

		void ShiftRegister(int r, Direction d, ShiftType type);
		uint8_t ShiftByte(uint8_t value, Direction d, ShiftType type);
};
