#include <sstream>
#include <iomanip>

#include "Core/Z80.h"
#include "Logging.h"

#define REGISTER_STATE hex << "A=" << unsigned(RegisterFile[A]) << \
		" B=" << unsigned(RegisterFile[B]) << \
		" C=" << unsigned(RegisterFile[C]) << \
		" D=" << unsigned(RegisterFile[D]) << \
		" E=" << unsigned(RegisterFile[E]) << dec

using namespace std;

Z80::Z80() : 
	bCGB(true)
{
	Reset(bCGB);
}

void Z80::Reset(bool bCGB)
{
	// Initial state copied from BGB
	this->bCGB = bCGB;

	memset(registerFile, 0, 8);
	PC = 0x100;
	SP = 0xFFFE;

	interruptMasterEnable = true;
	isHalted = false;
	isStopped = false;
	disablePCAdvance = false;
	calls_on_stack = 0;
	push_pop_balance = 0;

	if (bCGB)
	{
		registerFile[A] = 0x11;
		registerFile[D] = 0xFF;
		registerFile[E] = 0x56;
		registerFile[L] = 0x0D;
		registerFile[F] = 0x80;
	}
	else
	{
		registerFile[A] = 0x01;
		registerFile[C] = 0x13;
		registerFile[E] = 0xD8;
		registerFile[F] = 0xB0;
		registerFile[H] = 0x01;
		registerFile[L] = 0x4D;
	}

	if (interrupts)
		interrupts->Reset();
}

void Z80::SetMMU(std::shared_ptr<MMU> ptr)
{
	mmu = ptr;
	interrupts = ptr->GetInterruptController();
}

// Returns how many M cycles it took to execute the instruction
int Z80::Execute(uint16_t inst)
{
	OpCode opcode = static_cast<OpCode>(inst);
	int m_cycles = 0;

#ifdef ENABLE_VERBOSE_LOGGING
	stringstream verbose_log_message;
#endif

	switch (opcode)
	{
		case EXT:
		{
			uint8_t next_byte = mmu->ReadByte(++PC);
			return Execute(0xCB00 | next_byte);
		}

		case ADCA:
		case ADCB:
		case ADCC:
		case ADCD:
		case ADCE:
		case ADCH:
		case ADCL:
		case ADCHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			registerFile[A] = AddBytes(registerFile[A], value, true); // with_carry = true

			break;
		}

		case ADCn:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			registerFile[A] = AddBytes(registerFile[A], value, true); // with_carry = true

			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}

		case ADDA:
		case ADDB:
		case ADDC:
		case ADDD:
		case ADDE:
		case ADDH:
		case ADDL:
		case ADDHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			registerFile[A] = AddBytes(registerFile[A], value, false); // with_carry = false

			break;
		}

		case ADDn:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			registerFile[A] = AddBytes(registerFile[A], value, false); // with_carry = false

			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}

		case ADDHL_BC:
		case ADDHL_DE:
		case ADDHL_HL:
		case ADDHL_SP:
		{
			uint8_t regpair = (inst & 0x30) >> 4;
			uint16_t value = 0;

			switch (regpair)
			{
				case BC_MASK:
					value = ConcatRegisterPair(B, C);
					break;
				case DE_MASK:
					value = ConcatRegisterPair(D, E);
					break;
				case HL_MASK:
					value = ConcatRegisterPair(H, L);
					break;
				case SP_MASK:
					value = SP;
					break;
			}

			uint16_t hl = ConcatRegisterPair(H, L);
			uint32_t result = hl + value;
			uint16_t truncated_result = result & 0xFFFF;
			uint16_t xored = hl ^ value ^ truncated_result;

			// Carry and half-carry calculation rules are changed when adding 2 16 bit integers
			// C is set if carry occurs from bit 15 to bit 16
			SetCarryFlag((result & 0x10000) == 0x10000);
			// And H is set if carry occurs from bit 11 to bit 12
			SetHalfCarryFlag((xored & 0x1000) == 0x1000);
			SetOperationFlag(false);

			registerFile[H] = uint8_t((truncated_result & 0xFF00) >> 8);
			registerFile[L] = uint8_t(truncated_result & 0x00FF);

			m_cycles = 2;
			break;
		}

		case ADDSP_n:
		{
			int8_t value = int8_t(mmu->ReadByte(PC + 1));
			uint32_t result = SP + value;
			uint16_t truncated_result = result & 0xFFFF;
			uint16_t xored = SP ^ value ^ truncated_result;

			SetCarryFlag((xored & 0x100) == 0x100);
			SetHalfCarryFlag((xored & 0x10) == 0x10);

			SP = truncated_result;

			SetZeroFlag(false);
			SetOperationFlag(false);

			PC++;
			m_cycles = 4;
			break;
		}

		case ANDA:
		case ANDB:
		case ANDC:
		case ANDD:
		case ANDE:
		case ANDH:
		case ANDL:
		case ANDHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			registerFile[A] &= value;
			SetZeroFlag(registerFile[A] == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(true);
			SetCarryFlag(false);

			break;
		}

		case ANDN:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			registerFile[A] &= value;
			SetZeroFlag(registerFile[A] == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(true);
			SetCarryFlag(false);
			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}


		case ORA:
		case ORB:
		case ORC:
		case ORD:
		case ORE:
		case ORH:
		case ORL:
		case ORHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			registerFile[A] |= value;
			SetZeroFlag(registerFile[A] == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			SetCarryFlag(false);

			break;
		}

		case ORn:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			registerFile[A] |= value;
			SetZeroFlag(registerFile[A] == 0);
			SetCarryFlag(false);
			SetHalfCarryFlag(false);
			SetOperationFlag(false);
			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}

		case XORB:
		case XORC:
		case XORD:
		case XORE:
		case XORH:
		case XORL:
		case XORA:
		case XORHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			registerFile[A] ^= value;
			SetZeroFlag(registerFile[A] == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			SetCarryFlag(false);

			break;
		}

		case XORn:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			registerFile[A] ^= value;
			SetZeroFlag(registerFile[A] == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			SetCarryFlag(false);
			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}

		case BIT_0A:
		case BIT_0B:
		case BIT_0C:
		case BIT_0D:
		case BIT_0E:
		case BIT_0H:
		case BIT_0L:
		case BIT_1A:
		case BIT_1B:
		case BIT_1C:
		case BIT_1D:
		case BIT_1E:
		case BIT_1H:
		case BIT_1L:
		case BIT_2A:
		case BIT_2B:
		case BIT_2C:
		case BIT_2D:
		case BIT_2E:
		case BIT_2H:
		case BIT_2L:
		case BIT_3A:
		case BIT_3B:
		case BIT_3C:
		case BIT_3D:
		case BIT_3E:
		case BIT_3H:
		case BIT_3L:
		case BIT_4A:
		case BIT_4B:
		case BIT_4C:
		case BIT_4D:
		case BIT_4E:
		case BIT_4H:
		case BIT_4L:
		case BIT_5A:
		case BIT_5B:
		case BIT_5C:
		case BIT_5D:
		case BIT_5E:
		case BIT_5H:
		case BIT_5L:
		case BIT_6A:
		case BIT_6B:
		case BIT_6C:
		case BIT_6D:
		case BIT_6E:
		case BIT_6H:
		case BIT_6L:
		case BIT_7A:
		case BIT_7B:
		case BIT_7C:
		case BIT_7D:
		case BIT_7E:
		case BIT_7H:
		case BIT_7L:
		case BIT_0HL:
		case BIT_1HL:
		case BIT_2HL:
		case BIT_3HL:
		case BIT_4HL:
		case BIT_5HL:
		case BIT_6HL:
		case BIT_7HL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t bit_index = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				uint8_t value = ReadByteByRegPair(H, L);
				TestBit(bit_index, value);
				m_cycles = 3;
			}
			else
			{
				TestBit(bit_index, registerFile[data_loc]);
				m_cycles = 2;
			}

			// TestBit takes care of the zero flag
			SetOperationFlag(false);
			SetHalfCarryFlag(true);

			break;
		}

		case CALLZ_nn:
		case CALLNZ_nn:
		case CALLC_nn:
		case CALLNC_nn:
		case CALL:
		{
			uint8_t condition_code = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			bool condition_met = false;

			if (inst == OpCode::CALL)
			{
				condition_met = true; // CALL unconditionally with this instruction
			}
			else
			{
				// In the case of the conditional CALL instructions, we evaluate if the condition is met or not
				switch (condition_code)
				{
				case 0x0: // no zero
					condition_met = GetZeroFlag() == 0;
					break;
				case 0x1: // zero
					condition_met = GetZeroFlag() == 1;
					break;
				case 0x2: // no carry
					condition_met = GetCarryFlag() == 0;
					break;
				case 0x3: // carry
					condition_met = GetCarryFlag() == 1;
					break;
				}
			}

			if (condition_met)
			{
				uint16_t target_loworder = uint16_t(mmu->ReadByte(PC + 1));
				uint16_t target_highorder = uint16_t(mmu->ReadByte(PC + 2));

				// Push (PC + 3) onto the stack. So when RET is called, execution continues from after CALL (which consumes 3 bytes)
				mmu->WriteWord(SP - 2, PC + 3);
				SP -= 2;

				PC = (target_highorder << 8) + target_loworder;
				disablePCAdvance = true;

				m_cycles = 6;

#ifdef ENABLE_VERBOSE_LOGGING
				verbose_log_message << " (0x" << setw(4) << setfill('0') << PC << ") (" << dec << ++calls_on_stack << ")";
#endif
			}
			else
			{
				PC += 2;
				m_cycles = 3;
			}

			break;
		}

		case CCF:
		{
			SetCarryFlag(GetCarryFlag() == 0);
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			m_cycles = 1;
			break;
		}

		case CPA:
		case CPB:
		case CPC:
		case CPD:
		case CPE:
		case CPH:
		case CPL:
		case CPHL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;
			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			uint8_t compared = (registerFile[A] - value) & 0xFF;
			SetCarryFlag(registerFile[A] < value);
			SetZeroFlag(registerFile[A] == value);
			CalculateHalfCarry(registerFile[A], value, compared);
			SetOperationFlag(true);

			break;
		}

		case CPn:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			uint8_t compared = (registerFile[A] - value) & 0xFF;
			SetCarryFlag(registerFile[A] < value);
			SetZeroFlag(registerFile[A] == value);
			//SetHalfCarryFlag(((RegisterFile[A] ^ value ^ compared) & 0x10) == 0x10);
			CalculateHalfCarry(registerFile[A], value, compared);
			SetOperationFlag(true);

			PC++; // increment PC to skip the immediate value

			m_cycles = 2;
			break;
		}

		case CPRA:
		{
			registerFile[A] = ~registerFile[A];
			SetOperationFlag(true);
			SetHalfCarryFlag(true);
			m_cycles = 1;
			break;
		}


		// Adapted from AWJ's post on this nesdev thread:
		// http://forums.nesdev.com/viewtopic.php?f=20&t=15944
		case DAA:
		{
			int acc = registerFile[A];

			if (GetOperationFlag() == 0)
			{
				if (GetCarryFlag() == 1 || acc > 0x99)
				{
					acc += 0x60;
					SetCarryFlag(true);
				}

				if (GetHalfCarryFlag() == 1 || (acc & 0x0F) > 9)
				{
					acc += 0x06;
				}
			}
			else
			{
				if (GetCarryFlag() == 1)
				{
					acc -= 0x60;
				}

				if (GetHalfCarryFlag() == 1)
				{
					acc -= 0x06;
				}
			}

			registerFile[A] = acc & 0xFF;

			SetHalfCarryFlag(false);
			SetZeroFlag(registerFile[A] == 0);

			m_cycles = 1;

			break;
		}

		case DECA:
		case DECB:
		case DECC:
		case DECD:
		case DECE:
		case DECH:
		case DECL:
		case DECmHL:
		{
			uint8_t data_loc = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			uint8_t value;
			uint8_t decremented;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				decremented = value + int8_t(-1);
				WriteByteByRegPair(H, L, decremented);
				m_cycles = 3;
			}
			else
			{
				value = registerFile[data_loc];
				decremented = value + int8_t(-1);
				registerFile[data_loc] = decremented;
				m_cycles = 1;
			}

			SetHalfCarryFlag((value & 0xF) < 1);
			SetZeroFlag(decremented == 0);
			SetOperationFlag(true);

			break;
		}

		case DECBC:
		case DECDE:
		case DECHL:
		case DECSP:
		{
			uint8_t regpair = MaskAndShiftRight(uint8_t(inst), 0x30, 4);
			uint16_t value;

			switch (regpair)
			{
				case BC_MASK:
					value = ConcatRegisterPair(B, C);
					value = value + int16_t(-1);
					WriteWordToRegisterPair(B, C, value);
					break;
				case DE_MASK:
					value = ConcatRegisterPair(D, E);
					value = value + int16_t(-1);
					WriteWordToRegisterPair(D, E, value);
					break;
				case HL_MASK:
					value = ConcatRegisterPair(H, L);
					value = value + int16_t(-1);
					WriteWordToRegisterPair(H, L, value);
					break;
				case SP_MASK:
					SP = SP + int16_t(-1);
					break;
			}

			/* these instructions have no effect on the flag register */
			m_cycles = 2;
			break;
		}

		case DI:
		{
			interruptMasterEnable = false;
			m_cycles = 1;
			break;
		}

		case EI:
		{
			interruptMasterEnable = true;
			m_cycles = 1;
			break;
		}

		case HALT:
		{
			// TODO: return half a cycle in double speed mode
			isHalted = true;
			m_cycles = 1;
			break;
		}

		case INCB:
		case INCC:
		case INCD:
		case INCE:
		case INCH:
		case INCL:
		case INCA:
		case INCmHL:
		{
			uint8_t data_loc = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			uint8_t value;
			uint8_t incremented;

			if (data_loc == 0x6)
			{
				value = ReadByteByRegPair(H, L);
				incremented = value + 1;
				WriteByteByRegPair(H, L, incremented);
				m_cycles = 3;
			}
			else
			{
				value = registerFile[data_loc];
				incremented = ++registerFile[data_loc];
				m_cycles = 1;
			}

			CalculateHalfCarry(value, 1, incremented);
			SetZeroFlag(incremented == 0);
			SetOperationFlag(false);

			break;
		}

		case INCSP:
		case INCHL:
		case INCDE:
		case INCBC:
		{
			uint8_t regpair = MaskAndShiftRight(uint8_t(inst), 0x30, 4);
			uint16_t value;

			switch (regpair)
			{
				case BC_MASK:
					value = ConcatRegisterPair(B, C);
					value = value + 1;
					WriteWordToRegisterPair(B, C, value);
					break;
				case DE_MASK:
					value = ConcatRegisterPair(D, E);
					value = value + 1;
					WriteWordToRegisterPair(D, E, value);
					break;
				case HL_MASK:
					value = ConcatRegisterPair(H, L);
					value = value + 1;
					WriteWordToRegisterPair(H, L, value);
					break;
				case SP_MASK:
					SP = SP + 1;
					break;
			}

			/* these instructions have no effect on the flag register */
			m_cycles = 2;
			break;
		}

		case JPNZ_nn:
		case JPZ_nn:
		case JPNC_nn:
		case JPC_nn:
		{
			uint8_t condition_code = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			bool condition_met = false;

			switch (condition_code)
			{
				case 0x0: // no zero
					condition_met = GetZeroFlag() == 0;
					break;
				case 0x1: // zero
					condition_met = GetZeroFlag() == 1;
					break;
				case 0x2: // no carry
					condition_met = GetCarryFlag() == 0;
					break;
				case 0x3: // carry
					condition_met = GetCarryFlag() == 1;
					break;
			}

			if (condition_met)
			{
				uint16_t target_loworder = uint16_t(mmu->ReadByte(PC + 1));
				uint16_t target_highorder = uint16_t(mmu->ReadByte(PC + 2));
				PC = (target_highorder << 8) | target_loworder;
				disablePCAdvance = true;
				m_cycles = 4;
			}
			else
			{
				PC += 2; // advance the program counter to ignore these values
				m_cycles = 3;
			}

			break;
		}

		case JP_nn:
		{
			uint16_t target_loworder = uint16_t(mmu->ReadByte(PC + 1));
			uint16_t target_highorder = uint16_t(mmu->ReadByte(PC + 2));
			PC = (target_highorder << 8) | target_loworder;
			m_cycles = 4;
			disablePCAdvance = true;
			break;
		}

		case JPHL:
		{
			PC = ConcatRegisterPair(H, L);
			m_cycles = 1;
			disablePCAdvance = true;
			break;
		}

		case JR_n: // PC <- PC + e
		{
			int8_t offset = int8_t(mmu->ReadByte(PC + 1));
			PC = PC + offset + 2;
			m_cycles = 3;
			disablePCAdvance = true;
			break;
		}

		case JRNZ_n:
		case JRZ_n:
		case JRNC_n:
		case JRC_n:
		{
			uint8_t condition_code = MaskAndShiftRight(uint8_t(inst), 0x18, 3);
			bool condition_met = false;

			switch (condition_code)
			{
				case 0x0: // no zero
					condition_met = GetZeroFlag() == 0;
					break;
				case 0x1: // zero
					condition_met = GetZeroFlag() == 1;
					break;
				case 0x2: // no carry
					condition_met = GetCarryFlag() == 0;
					break;
				case 0x3: // carry
					condition_met = GetCarryFlag() == 1;
					break;
			}

			if (condition_met)
			{
				int8_t offset = int8_t(mmu->ReadByte(PC + 1));
				PC = PC + offset + 2;
				disablePCAdvance = true;
				m_cycles = 3;
			}
			else
			{
				PC++; // advance program counter to ignore immediate value
				m_cycles = 2;
			}

			break;
		}
		
		case LDBC_nn:
		case LDDE_nn:
		case LDHL_nn:
		case LDSP_nn:
		{
			uint8_t regpair = MaskAndShiftRight(uint8_t(inst), 0x30, 4);

			// The byte immediately following the opcode is the low order byte, followed by the high order byte
			// That's why they are swapped around in the SP case
			switch (regpair)
			{
				case BC_MASK:
					registerFile[C] = mmu->ReadByte(PC + 1);
					registerFile[B] = mmu->ReadByte(PC + 2);
					break;
				case DE_MASK:
					registerFile[E] = mmu->ReadByte(PC + 1);
					registerFile[D] = mmu->ReadByte(PC + 2);
					break;
				case HL_MASK:
					registerFile[L] = mmu->ReadByte(PC + 1);
					registerFile[H] = mmu->ReadByte(PC + 2);
					break;
				case SP_MASK:
					SP = uint16_t(mmu->ReadByte(PC + 2) << 8) | uint16_t(mmu->ReadByte(PC + 1));
					break;
			}

			PC += 2;
			m_cycles = 3;
			break;
		}

		case LDB_n:
		case LDC_n:
		case LDD_n:
		case LDE_n:
		case LDH_n:
		case LDL_n:
		case LDA_n:
		{
			uint8_t r = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			registerFile[r] = mmu->ReadByte(PC + 1);
			PC++;
			m_cycles = 2;
			break;
		}

		case LDB_B:
		case LDB_C:
		case LDB_D:
		case LDB_E:
		case LDB_H:
		case LDB_L:
		case LDB_A:
		case LDC_B:
		case LDC_C:
		case LDC_D:
		case LDC_E:
		case LDC_H:
		case LDC_L:
		case LDC_A:
		case LDD_B:
		case LDD_C:
		case LDD_D:
		case LDD_E:
		case LDD_H:
		case LDD_L:
		case LDD_A:
		case LDE_B:
		case LDE_C:
		case LDE_D:
		case LDE_E:
		case LDE_H:
		case LDE_L:
		case LDE_A:
		case LDH_B:
		case LDH_C:
		case LDH_D:
		case LDH_E:
		case LDH_H:
		case LDH_L:
		case LDH_A:
		case LDL_B:
		case LDL_C:
		case LDL_D:
		case LDL_E:
		case LDL_H:
		case LDL_L:
		case LDL_A:
		case LDA_B:
		case LDA_C:
		case LDA_D:
		case LDA_E:
		case LDA_H:
		case LDA_L:
		case LDA_A:
		{
			uint8_t src_reg = inst & 0x7;
			uint8_t dest_reg = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			registerFile[dest_reg] = registerFile[src_reg];
			m_cycles = 1;
			break;
		}

		case LDBC_A:
		{
			mmu->WriteByte(ConcatRegisterPair(B, C), registerFile[A]);
			m_cycles = 2;
			break;
		}

		case LDDE_A:
		{
			mmu->WriteByte(ConcatRegisterPair(D, E), registerFile[A]);
			m_cycles = 2;
			break;
		}

		case LDHL_B:
		case LDHL_C:
		case LDHL_D:
		case LDHL_E:
		case LDHL_H:
		case LDHL_L:
		case LDHL_A:
		{
			uint8_t r = inst & 0x7;
			WriteByteByRegPair(H, L, registerFile[r]);
			m_cycles = 2;
			break;
		}

		case LDA_BC:
		{
			registerFile[A] = ReadByteByRegPair(B, C);
			m_cycles = 2;
			break;
		}

		case LDA_DE:
		{
			registerFile[A] = ReadByteByRegPair(D, E);
			m_cycles = 2;
			break;
		}

		case LDB_HL:
		case LDC_HL:
		case LDD_HL:
		case LDE_HL:
		case LDH_HL:
		case LDL_HL:
		case LDA_HL:
		{
			uint8_t r = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			registerFile[r] = ReadByteByRegPair(H, L);
			m_cycles = 2;
			break;
		}

		case LDnn_A:
		{
			uint16_t addr = uint16_t(mmu->ReadByte(PC + 2) << 8) + uint16_t(mmu->ReadByte(PC + 1));
			mmu->WriteByte(addr, registerFile[A]);
			PC += 2;
			m_cycles = 4;
			break;
		}

		case LDA_nn:
		{
			uint16_t addr = uint16_t(mmu->ReadByte(PC + 2) << 8) + uint16_t(mmu->ReadByte(PC + 1));
			registerFile[A] = mmu->ReadByte(addr);
			PC += 2;
			m_cycles = 4;
			break;
		}

		case LDnn_SP:
		{
			uint16_t addr = uint16_t(mmu->ReadByte(PC + 2) << 8) + uint16_t(mmu->ReadByte(PC + 1));
			mmu->WriteWord(addr, SP);
			PC += 2;
			m_cycles = 5;
			break;
		}

		case LDIHL_A:
		case LDIA_HL:
		case LDDHL_A:
		case LDDA_HL:
		{
			uint16_t concat = ConcatRegisterPair(H, L);

			switch (inst)
			{
				case 0x22: // LDIHL_A
					mmu->WriteByte(concat, registerFile[A]);
					concat++;
					break;
				case 0x2A: // LDIA_HL
					registerFile[A] = mmu->ReadByte(concat);
					concat++;
					break;
				case 0x32: // LDDHL_A
					mmu->WriteByte(concat, registerFile[A]);
					concat--;
					break;
				case 0x3A: // LDDA_HL
					registerFile[A] = mmu->ReadByte(concat);
					concat--;
					break;
			}

			registerFile[H] = uint8_t((concat & 0xFF00) >> 8);
			registerFile[L] = uint8_t(concat & 0x00FF);
			m_cycles = 2;
			break;
		}

		case LDHL_n:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			WriteByteByRegPair(H, L, value);
			PC++; // Ignore immediate
			m_cycles = 3;
			break;
		}

		case LDhn_A:
		{
			uint16_t addr = 0xFF00 + mmu->ReadByte(PC + 1);
			mmu->WriteByte(addr, registerFile[A]);
			PC++; // Ignore immediate
			m_cycles = 3;
			break;
		}

		case LDhC_A:
		{
			uint16_t addr = 0xFF00 + registerFile[C];
			mmu->WriteByte(addr, registerFile[A]);
			m_cycles = 2;
			break;
		}

		case LDhA_n:
		{
			uint16_t addr = 0xFF00 + mmu->ReadByte(PC + 1);
			registerFile[A] = mmu->ReadByte(addr);
			PC++; // Ignore immediate
			m_cycles = 3;
			break;
		}

		case LDhA_C:
		{
			uint16_t addr = 0xFF00 + registerFile[C];
			registerFile[A] = mmu->ReadByte(addr);
			m_cycles = 2;
			break;
		}

		case LDHL_SPd:
		{
			int8_t value = int8_t(mmu->ReadByte(PC + 1));

			uint32_t result = SP + value;
			uint16_t truncated_result = result & 0xFFFF;
			uint16_t xored = SP ^ value ^ truncated_result;

			SetCarryFlag((xored & 0x100) == 0x100);
			SetHalfCarryFlag((xored & 0x10) == 0x10);
			// For whatever reason, zero flag is cleared by this instruction
			SetZeroFlag(false);
			SetOperationFlag(false);

			registerFile[H] = uint8_t((truncated_result & 0xFF00) >> 8);
			registerFile[L] = uint8_t(truncated_result & 0x00FF);

			PC++;
			m_cycles = 3;
			break;
		}

		case LDSP_HL:
		{
			SP = ConcatRegisterPair(H, L);
			m_cycles = 2;
			break;
		}

		case NOP:
		{
			m_cycles = 1;
			break;
		}


		case POP_BC:
		case POP_DE:
		case POP_HL:
		case POP_AF:
		{
			uint8_t pair = MaskAndShiftRight(uint8_t(inst), 0x30, 4);

			switch (pair)
			{
				case BC_MASK:
					registerFile[C] = mmu->ReadByte(SP);
					registerFile[B] = mmu->ReadByte(SP + 1);
					break;
				case DE_MASK:
					registerFile[E] = mmu->ReadByte(SP);
					registerFile[D] = mmu->ReadByte(SP + 1);
					break;
				case HL_MASK:
					registerFile[L] = mmu->ReadByte(SP);
					registerFile[H] = mmu->ReadByte(SP + 1);
					break;
				case AF_MASK:
					registerFile[F] = mmu->ReadByte(SP) & 0xF0;
					registerFile[A] = mmu->ReadByte(SP + 1);
					break;
			}


#ifdef ENABLE_VERBOSE_LOGGING
			verbose_log_message << dec << showpos << "(" << --push_pop_balance << ")";
#endif

			SP += 2;
			m_cycles = 3;

			break;
		}

		case PUSH_BC:
		case PUSH_DE:
		case PUSH_HL:
		case PUSH_AF:
		{
			uint8_t pair = MaskAndShiftRight(uint8_t(inst), 0x30, 4);

			switch (pair)
			{
				case BC_MASK:
					mmu->WriteByte(SP - 1, registerFile[B]);
					mmu->WriteByte(SP - 2, registerFile[C]);
					break;
				case DE_MASK:
					mmu->WriteByte(SP - 1, registerFile[D]);
					mmu->WriteByte(SP - 2, registerFile[E]);
					break;
				case HL_MASK:
					mmu->WriteByte(SP - 1, registerFile[H]);
					mmu->WriteByte(SP - 2, registerFile[L]);
					break;
				case AF_MASK:
					mmu->WriteByte(SP - 1, registerFile[A]);
					mmu->WriteByte(SP - 2, registerFile[F]);
					break;
			}

#ifdef ENABLE_VERBOSE_LOGGING
			verbose_log_message << dec << showpos << "(" << ++push_pop_balance << ")";
#endif

			SP -= 2;
			m_cycles = 4;

			break;
		}

		case RETNZ:
		case RETZ:
		case RETNC:
		case RETC:
		case RET:
		{
			uint8_t condition_code = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			bool condition_met = false;

			if (inst == OpCode::RET)
			{
				condition_met = true; // RET unconditionally with this instruction
			}
			else
			{
				// In the case of the conditional CALL instructions, we evaluate if the condition is met or not
				switch (condition_code)
				{
					case 0x0: // no zero
						condition_met = GetZeroFlag() == 0;
						break;
					case 0x1: // zero
						condition_met = GetZeroFlag() == 1;
						break;
					case 0x2: // no carry
						condition_met = GetCarryFlag() == 0;
						break;
					case 0x3: // carry
						condition_met = GetCarryFlag() == 1;
						break;
				}
			}

			if (condition_met)
			{
				uint16_t target_low = uint16_t(mmu->ReadByte(SP));
				uint16_t target_high = uint16_t(mmu->ReadByte(SP + 1));

				PC = (target_high << 8) | target_low;
				SP += 2;
				m_cycles = 5;
				disablePCAdvance = true;

				if (inst == RET) m_cycles = 4;

#ifdef ENABLE_VERBOSE_LOGGING
				verbose_log_message << " (0x" << setw(4) << setfill('0') << PC << ") (" << dec << calls_on_stack-- << ")";
#endif
			}
			else
			{
				m_cycles = 2;
			}

			break;
		}

		case RETI:
		{
			uint16_t target_low = uint16_t(mmu->ReadByte(SP));
			uint16_t target_high = uint16_t(mmu->ReadByte(SP + 1));

			PC = (target_high << 8) + target_low;
			SP += 2;
			interruptMasterEnable = true;
			disablePCAdvance = true;

			m_cycles = 4;

			break;
		}


		case RLC_B:
		case RLC_C:
		case RLC_D:
		case RLC_E:
		case RLC_H:
		case RLC_L:
		case RLC_HL:
		case RLC_A:
		case RRC_B:
		case RRC_C:
		case RRC_D:
		case RRC_E:
		case RRC_H:
		case RRC_L:
		case RRC_HL:
		case RRC_A:
		case RLB:
		case RLC:
		case RLD:
		case RLE:
		case RLH:
		case RLL:
		case RLHL:
		case RLA:
		case RRB:
		case RRC:
		case RRD:
		case RRE:
		case RRH:
		case RRL:
		case RRHL:
		case RRA:
		case RRC_A_2B:
		case RRA_2B:
		case RLA_2B:
		case RLC_A_2B:
		{
			uint8_t data_loc = inst & 0x7;

			// Bit 3 indicates direction in these instructions
			Direction dir = (inst & 0x8) == 0x8 ? Direction::Right : Direction::Left;

			// Bit 4 indicates carry behavior in these instructions
			RotateCarryBehavior carry_behavior = (inst & 0x10) == 0x10 ? RotateCarryBehavior::ThroughCarry : RotateCarryBehavior::BranchCarry;

			if (data_loc == 0x6)
			{
				uint8_t value = ReadByteByRegPair(H, L);
				value = RotateByte(value, dir, carry_behavior);
				WriteByteByRegPair(H, L, value);
				SetZeroFlag(value == 0);
				m_cycles = 4;
			}
			else
			{
				RotateRegister(data_loc, dir, carry_behavior);

				if (inst == RLC_A_2B || inst == RRC_A || inst == RLA_2B || inst == RRA_2B)
				{
					m_cycles = 1;
					SetZeroFlag(false);
				}
				else
				{
					m_cycles = 2;
					SetZeroFlag(registerFile[data_loc] == 0);
				}
			}

			SetHalfCarryFlag(false);
			SetOperationFlag(false);
			break;
		}

		case SLAB:
		case SLAC:
		case SLAD:
		case SLAE:
		case SLAH:
		case SLAL:
		case SLAHL:
		case SLAA:
		case SRAB:
		case SRAC:
		case SRAD:
		case SRAE:
		case SRAH:
		case SRAL:
		case SRAHL:
		case SRAA:
		case SRLB:
		case SRLC:
		case SRLD:
		case SRLE:
		case SRLH:
		case SRLL:
		case SRLHL:
		case SRLA:
		{
			uint8_t data_loc = inst & 0x7;

			// Bit 3 indicates direction in these instructions
			Direction dir = (inst & 0x8) == 0x8 ? Direction::Right : Direction::Left;

			// Bit 4 indicates shift type in these instructions
			ShiftType shift_type = (inst & 0x10) == 0x10 ? ShiftType::Logical : ShiftType::Arithmetic;

			if (data_loc == 0x6)
			{
				uint8_t value = ReadByteByRegPair(H, L);
				value = ShiftByte(value, dir, shift_type);
				WriteByteByRegPair(H, L, value);

				SetZeroFlag(value == 0);
				m_cycles = 4;
			}
			else
			{
				ShiftRegister(data_loc, dir, shift_type);

				SetZeroFlag(registerFile[data_loc] == 0);
				m_cycles = 2;
			}

			SetHalfCarryFlag(false);
			SetOperationFlag(false);
			break;
		}

		case SCF:
		{
			SetCarryFlag(true);
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			m_cycles = 1;
			break;
		}

		case STOP:
		{
			if (mmu->GetCGBRegisters().GetPrepareSpeedSwitch())
				mmu->GetCGBRegisters().ToggleSpeed();
			else
				isStopped = true;

			m_cycles = 1;
			break;
		}

		case RST0:
		case RST10:
		case RST20:
		case RST30:
		case RST8:
		case RST18:
		case RST28:
		case RST38:
		{
			uint8_t p = MaskAndShiftRight(uint8_t(inst), 0x38, 3);

			mmu->WriteWord(SP - 2, PC + 1);
			SP -= 2;

			PC = p * 0x8;

			disablePCAdvance = true;
			m_cycles = 4;
			break;
		}

		case SBCA_B:
		case SBCA_C:
		case SBCA_D:
		case SBCA_E:
		case SBCA_H:
		case SBCA_L:
		case SBCA_HL:
		case SBCA_A:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			uint8_t carryin = GetCarryFlag();
			int16_t dirty = registerFile[A] - value - carryin;

			SetCarryFlag(dirty < 0);
			SetOperationFlag(true);
			SetHalfCarryFlag(false);
			SetHalfCarryFlag(((registerFile[A] & 0xF) - (value & 0xF) - carryin) < 0);

			registerFile[A] = dirty & 0xFF;
			SetZeroFlag(registerFile[A] == 0);

			break;
		}

		case SBCA_n:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			uint8_t carryin = GetCarryFlag();
			int32_t dirty = registerFile[A] - value - carryin;

			SetCarryFlag(dirty < 0);
			SetOperationFlag(true);
			SetHalfCarryFlag(((registerFile[A] & 0xF) - (value & 0xF) - carryin) < 0);
			registerFile[A] = dirty & 0xFF;
			SetZeroFlag(registerFile[A] == 0);

			m_cycles = 2;
			PC++;

			break;
		}

		case SUBA_B:
		case SUBA_C:
		case SUBA_D:
		case SUBA_E:
		case SUBA_H:
		case SUBA_L:
		case SUBA_HL:
		case SUBA_A:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t value = 0;

			if (data_loc == 0x6) // 0x6 indicates to use (HL) as value
			{
				value = ReadByteByRegPair(H, L);
				m_cycles = 2;
			}
			else
			{
				value = registerFile[data_loc];
				m_cycles = 1;
			}

			int32_t dirty = registerFile[A] - value;

			SetCarryFlag(dirty < 0);
			SetOperationFlag(true);
			SetZeroFlag(dirty == 0);
			CalculateHalfCarry(registerFile[A], value, dirty);

			registerFile[A] = dirty & 0xFF;

			break;
		}

		case SUBA_n:
		{
			uint8_t value = mmu->ReadByte(PC + 1);
			int32_t dirty = registerFile[A] - value;

			SetCarryFlag(dirty < 0);
			SetOperationFlag(true);
			SetZeroFlag(dirty == 0);
			CalculateHalfCarry(registerFile[A], value, dirty);

			registerFile[A] = dirty & 0xFF;

			PC++;
			m_cycles = 2;

			break;
		}

		case SWAPB:
		case SWAPC:
		case SWAPD:
		case SWAPE:
		case SWAPH:
		case SWAPL:
		case SWAPA:
		case SWAPHL:
		{
			uint8_t data_loc = inst & 0x7;
			if (data_loc == 0x6)
			{
				uint8_t value = ReadByteByRegPair(H, L);
				uint8_t result = (value & 0x0F) << 4 | (value & 0xF0) >> 4;
				WriteByteByRegPair(H, L, result);
				SetZeroFlag(result == 0);
				m_cycles = 4;
			}
			else
			{
				uint8_t value = registerFile[data_loc];
				registerFile[data_loc] = (value & 0x0F) << 4 | (value & 0xF0) >> 4;
				SetZeroFlag(registerFile[data_loc] == 0);
				m_cycles = 2; // TODO: these are guesses
			}
			
			SetOperationFlag(false);
			SetHalfCarryFlag(false);
			SetCarryFlag(false);

			break;
		}

		case RES_0B:
		case RES_0C:
		case RES_0D:
		case RES_0E:
		case RES_0H:
		case RES_0L:
		case RES_0A:
		case RES_1B:
		case RES_1C:
		case RES_1D:
		case RES_1E:
		case RES_1H:
		case RES_1L:
		case RES_1A:
		case RES_2B:
		case RES_2C:
		case RES_2D:
		case RES_2E:
		case RES_2H:
		case RES_2L:
		case RES_2A:
		case RES_3B:
		case RES_3C:
		case RES_3D:
		case RES_3E:
		case RES_3H:
		case RES_3L:
		case RES_3A:
		case RES_4B:
		case RES_4C:
		case RES_4D:
		case RES_4E:
		case RES_4H:
		case RES_4L:
		case RES_4A:
		case RES_5B:
		case RES_5C:
		case RES_5D:
		case RES_5E:
		case RES_5H:
		case RES_5L:
		case RES_5A:
		case RES_6B:
		case RES_6C:
		case RES_6D:
		case RES_6E:
		case RES_6H:
		case RES_6L:
		case RES_6A:
		case RES_7B:
		case RES_7C:
		case RES_7D:
		case RES_7E:
		case RES_7H:
		case RES_7L:
		case RES_7A:
		case RES_0HL:
		case RES_1HL:
		case RES_2HL:
		case RES_3HL:
		case RES_4HL:
		case RES_5HL:
		case RES_6HL:
		case RES_7HL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t b = MaskAndShiftRight(uint8_t(inst), 0x38, 3);
			if (data_loc == 0x6)
			{
				uint8_t value = ReadByteByRegPair(H, L);
				WriteByteByRegPair(H, L, ClearBit(b, value));
				m_cycles = 4;
			}
			else
			{
				ClearRegisterBit(b, data_loc);
				m_cycles = 2;
			}

			break;
		}

		case SET_0B:
		case SET_0C:
		case SET_0D:
		case SET_0E:
		case SET_0H:
		case SET_0L:
		case SET_0A:
		case SET_1B:
		case SET_1C:
		case SET_1D:
		case SET_1E:
		case SET_1H:
		case SET_1L:
		case SET_1A:
		case SET_2B:
		case SET_2C:
		case SET_2D:
		case SET_2E:
		case SET_2H:
		case SET_2L:
		case SET_2A:
		case SET_3B:
		case SET_3C:
		case SET_3D:
		case SET_3E:
		case SET_3H:
		case SET_3L:
		case SET_3A:
		case SET_4B:
		case SET_4C:
		case SET_4D:
		case SET_4E:
		case SET_4H:
		case SET_4L:
		case SET_4A:
		case SET_5B:
		case SET_5C:
		case SET_5D:
		case SET_5E:
		case SET_5H:
		case SET_5L:
		case SET_5A:
		case SET_6B:
		case SET_6C:
		case SET_6D:
		case SET_6E:
		case SET_6H:
		case SET_6L:
		case SET_6A:
		case SET_7B:
		case SET_7C:
		case SET_7D:
		case SET_7E:
		case SET_7H:
		case SET_7L:
		case SET_7A:
		case SET_0HL:
		case SET_1HL:
		case SET_2HL:
		case SET_3HL:
		case SET_4HL:
		case SET_5HL:
		case SET_6HL:
		case SET_7HL:
		{
			uint8_t data_loc = inst & 0x7;
			uint8_t b = MaskAndShiftRight(uint8_t(inst), 0x38, 3);

			if (data_loc == 0x6)
			{
				uint8_t value = ReadByteByRegPair(H, L);
				WriteByteByRegPair(H, L, SetBit(b, value));
				m_cycles = 4;
			}
			else
			{
				SetRegisterBit(b, data_loc);
				m_cycles = 2;
			}

			break;
		}
	}

	if (disablePCAdvance)
	{
		disablePCAdvance = false;
	}
	else
	{
		PC++;
	}

#ifdef ENABLE_VERBOSE_LOGGING
	LOG_VERBOSE("[Z80] %s %s", verbose_log_message.str(), REGISTER_STATE);
#endif

	return m_cycles;
}

// Returns how many M cycles it took to jump to an ISR
int Z80::HandleInterrupts()
{
	uint8_t pending_interrupts = interrupts->ReadEnabledInterrupts() & interrupts->ReadRequestedInterrupts();

	// Special note about the HALT instruction: When it is called the gameboy enters an 'idle' state and stops executing
	// instructions until an enabled interrupt is requested. If IME=1, then the gameboy services the interrupt, leaves the idle state
	// and thereby begins execution again at the address following the HALT. However, if IME=0, the gameboy leaves the idle state
	// but does NOT service the interrupt and instead continues execution at the address following the HALT.
	if (IsIdle() && pending_interrupts != 0x0)
	{
		// Leave the idle state
		if (isHalted) isHalted = false;
		if (isStopped) isStopped = false;
		
		if (!interruptMasterEnable) return 0;
	}
	
	if (!interruptMasterEnable) return 0;
	if (pending_interrupts == 0x0) return 0;

	interruptMasterEnable = false;

	// Now, we can jump to the ISR
	mmu->WriteWord(SP - 2, PC);
	SP -= 2;

	// Set the PC and disable the corresponding interrupt request
	if ((pending_interrupts & 0x1) == 0x1)
	{
		PC = 0x40; // VBlank ISR
		mmu->GetInterruptController()->VBlankRequested = false;
		LOG_VERBOSE("[Z80] VBlank Interrupt");
	}
	else if ((pending_interrupts & 0x2) == 0x2)
	{
		PC = 0x48; // LCD Status ISR
		mmu->GetInterruptController()->LCDStatusRequested = false;
		LOG_VERBOSE("[Z80] LCDStatus Interrupt");
	}
	else if ((pending_interrupts & 0x4) == 0x4)
	{
		PC = 0x50; // Timer ISR
		mmu->GetInterruptController()->TimerRequested = false;
		LOG_VERBOSE("[Z80] Timer Interrupt");
	}
	else if ((pending_interrupts & 0x8) == 0x8)
	{
		PC = 0x58; // Serial ISR
		mmu->GetInterruptController()->SerialRequested = false;
		LOG_VERBOSE("[Z80] Serial Interrupt");
	}
	else if ((pending_interrupts & 0x10) == 0x10)
	{
		PC = 0x60; // Joypad ISR
		mmu->GetInterruptController()->JoypadRequested = false;
		LOG_VERBOSE("[Z80] Joypad Interrupt");
	}
	else
		LOG_ERROR("[Z80] Unrecognized interrupt requested"); // This shouldn't happen, but I don't like not having an else clause

	// TODO: is it correct to assume we return 5 when we jump to an ISR and 0 otherwise?
	return 5;
}

uint8_t Z80::ReadByteByRegPair(uint8_t R0, uint8_t R1)
{
	return mmu->ReadByte((registerFile[R0] << 8) | registerFile[R1]);
}

inline void Z80::WriteByteByRegPair(uint8_t R0, uint8_t R1, uint8_t value)
{
	mmu->WriteByte((registerFile[R0] << 8) | registerFile[R1], value);
}

inline uint8_t Z80::AddBytes(uint8_t b1, uint8_t b2, bool with_carry)
{
	uint16_t result = b1 + b2 + (with_carry ? GetCarryFlag() : 0);
	uint8_t trucated_result = result & 0xFF;

	SetZeroFlag(trucated_result == 0);
	SetCarryFlag(((b1 ^ b2 ^ result) & 0x100) == 0x100);
	CalculateHalfCarry(b1, b2, result);
	SetOperationFlag(false);

	return trucated_result;

/*
NOTES ON HOW CARRY AND HALF-CARRY ARE CALCULATED:

When determining whether a carry occured from bit_n to bit_n+1
we can use a truth table to figure out how the calculation happens.

A is bit_n+1 for the first operand
B is bit_n+1 for the second operand
X is bit_n+1 for the result of the addition
Cin is the carry bit from the addition of bit_n for the first and second operand 
    (i.e. it indicates whether a carry was brought in to the n+1 column)

A  B  X  | Cin
0  0  0  |   0    (0+0=0, no carry in)
0  0  1  |   1    (the addition of 0 and 0 resulted in a 1, this means a carry must've been brought in)  
0  1  0  |   1    (the addition of 0 and 1 resulted in a 0, this means Cin was 1)
0  1  1  |   0    (0+1=1, no carry was brought in)
1  0  0  |   1    (similarly, if the addition of 1 and 0 resulted in a 0, a carry must've been brought in)
1  0  1  |   0    (1+0=1, no carry)
1  1  0  |   0    (1+1=0, no carry was brought in, but this will result in a carry-out from bit_n+1 to bit_n+2)
1  1  1  |   1    (the addition of a 1 and 1 resulted in a 1, a carry was brought in here also)

Note that Cin=1 when there are an odd number of a 1s on the left side of the table (for columns A, B and X).
In other words, Cin can be calculated like so:

Cin = A  xor  B  xor  X

Thus, the carry and halfcarry flags can be calculated by xor'ing the bits in the 9th and 5th columns respectively.

*/
}

inline uint8_t Z80::MaskAndShiftRight(uint8_t val, uint8_t mask, int shift)
{
	return (val & mask) >> shift;
}

inline uint8_t Z80::MaskAndShiftLeft(uint8_t val, uint8_t mask, int shift)
{
	return (val & mask) << shift;
}

inline uint16_t Z80::ConcatRegisterPair(uint8_t R0, uint8_t R1)
{
	return (registerFile[R0] << 8) | registerFile[R1];
}

inline void Z80::WriteWordToRegisterPair(uint8_t R0, uint8_t R1, uint16_t value)
{
	registerFile[R0] = (value & 0xFF00) >> 8;
	registerFile[R1] = value & 0x00FF;
}

inline void Z80::SetCarryFlag(uint8_t v)
{
	SetCarryFlag(v == 1);
}

inline void Z80::SetCarryFlag(bool bSet)
{
	if (bSet)
		registerFile[F] |= CARRY_MASK;
	else
		registerFile[F] &= ~CARRY_MASK;
}

void Z80::SetHalfCarryFlag(uint8_t v)
{
	SetHalfCarryFlag(v == 1);
}

inline void Z80::SetHalfCarryFlag(bool bSet)
{
	if (bSet)
		registerFile[F] |= HALF_CARRY_MASK;
	else
		registerFile[F] &= ~HALF_CARRY_MASK;
}

inline void Z80::CalculateHalfCarry(uint8_t opl, uint8_t opr, uint8_t res)
{
	SetHalfCarryFlag(((opl ^ opr ^ res) & 0x10) == 0x10);
}

inline void Z80::SetZeroFlag(uint8_t v)
{
	SetZeroFlag(v == 1);
}

inline void Z80::SetZeroFlag(bool bSet)
{
	if (bSet)
		registerFile[F] |= ZERO_MASK;
	else
		registerFile[F] &= ~ZERO_MASK;
}

inline void Z80::SetOperationFlag(uint8_t v)
{
	SetOperationFlag(v == 1);
}

inline void Z80::SetOperationFlag(bool bSet)
{
	if (bSet)
		registerFile[F] |= OPERATION_MASK;
	else
		registerFile[F] &= ~OPERATION_MASK;
}

void Z80::TestBit(int b, uint8_t val)
{
	uint8_t t = (val & (0x01 << b)) >> b;
	SetZeroFlag(t == 0);
}

inline uint8_t Z80::ClearBit(int b, uint8_t value)
{
	return value & (~(0x01 << b));
}

inline uint8_t Z80::SetBit(int b, uint8_t value)
{
	return value | (0x01 << b);
}

inline void Z80::SetRegisterBit(int b, int r)
{
	registerFile[r] |= (0x01 << b);
}

inline void Z80::ClearRegisterBit(int b, int r)
{
	registerFile[r] &= (~(0x01 << b));
}

inline void Z80::RotateRegister(int r, Direction d, RotateCarryBehavior carry_behaviour)
{
	registerFile[r] = RotateByte(registerFile[r], d, carry_behaviour);
}

inline uint8_t Z80::RotateByte(uint8_t value, Direction d, RotateCarryBehavior carry_behaviour)
{
	// Cast to a larger integer to give some room when rotating
	uint16_t value_16 = uint16_t(value);

	if (d == Direction::Left)
	{
		uint8_t left_most_bit = 0;
		if (carry_behaviour == RotateCarryBehavior::BranchCarry)
		{
			left_most_bit = MaskAndShiftRight(value, 0x80, 7);
			value_16 <<= 1;
			value_16 += left_most_bit;
			SetCarryFlag(left_most_bit);
		}
		else
		{
			left_most_bit = GetCarryFlag();
			SetCarryFlag((value & 0x80) == 0x80); // This takes care of rotating through the carry
			value_16 <<= 1;
			value_16 += left_most_bit;
		}

	}
	else
	{
		uint8_t right_most_bit = 0;

		if (carry_behaviour == RotateCarryBehavior::BranchCarry)
		{
			right_most_bit = value & 0x1;
			value_16 >>= 1;
			value_16 += (right_most_bit << 7); // Place the right-most-bit at the highest bit's place
			SetCarryFlag(right_most_bit);
		}
		else
		{
			right_most_bit = GetCarryFlag();
			SetCarryFlag((value & 0x1) == 0x1); // This takes care of rotating through the carry
			value_16 >>= 1;
			value_16 += (right_most_bit << 7);
		}
	}

	return uint8_t(value_16 & 0xFF);
}

inline void Z80::ShiftRegister(int r, Direction dir, ShiftType type)
{
	registerFile[r] = ShiftByte(registerFile[r], dir, type);
}

inline uint8_t Z80::ShiftByte(uint8_t value, Direction dir, ShiftType type)
{
	if (dir == Direction::Left)
	{
		if (type == ShiftType::Arithmetic)
		{
			SetCarryFlag((value & 0x80) == 0x80);
			value <<= 1;
		}
		else
		{
			LOG_ERROR("[Z80] Left shifts cannot be logical");
		}
	}
	else
	{
		SetCarryFlag((value & 0x01) == 0x01);

		if (type == ShiftType::Arithmetic)
		{
			uint8_t left_most_bit = value & 0x80;
			value >>= 1;
			value |= left_most_bit;
		}
		else
		{
			value >>= 1;
		}
	}

	return value;
}