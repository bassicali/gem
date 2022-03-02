#pragma once
#include <string>

enum class Direction
{
	Left = 0,
	Right = 1,
};

enum class RotateCarryBehavior
{
	ThroughCarry = 0,
	BranchCarry = 1,
};

enum class ShiftType
{
	Arithmetic = 0,
	Logical = 1,
};

/*
** NOTE: there are discrepancies in the timings for the following instructions
** between Zak's Book and the Pan Docs
** JP_nn
** all CALLs
*/

struct OpCodeInfo
{
	OpCodeInfo()
		: OpCode(0)

	{
	}
	
	OpCodeInfo(uint16_t opcode, std::string enum_name, std::string mnemonic)
		: EnumName(enum_name)
		, OpCode(opcode)
		, Mnemonic(mnemonic)
	{
	}
	std::string EnumName;
	uint16_t OpCode;
	std::string Mnemonic;
};

class OpCodeIndex
{
public:
	OpCodeIndex();
	~OpCodeIndex();

	bool Lookup(uint16_t opcode, OpCodeInfo& info);
	bool Contains(uint16_t opcode);
	int GetImmSize(uint16_t inst);

	OpCodeInfo& operator[](int index);

	static OpCodeIndex& Get();

private:
	short* usedIndices;
	short* opcodeToIdxMap;
	OpCodeInfo* list;
};

enum OpCode
{
	/* Special instruction that redirections to the CBXX instructions */
	EXT = 0xCB,

	/* Add with carry operations */
	ADCB = 0x88, // A <- A + B + carry
	ADCC = 0x89, // A <- A + C + carry
	ADCD = 0x8A, // A <- A + D + carry
	ADCE = 0x8B, // A <- A + E + carry
	ADCH = 0x8C, // A <- A + H + carry
	ADCL = 0x8D, // A <- A + L + carry
	ADCHL = 0x8E, // A <- A + [HL] + carry
	ADCA = 0x8F, // A <- A + A + carry
	ADCn = 0xCE, // A <- A + n + carry


				 /* Add without carry operations */
	ADDB = 0x80, // A <- A + B
	ADDC = 0x81, // A <- A + C
	ADDD = 0x82, // A <- A + D
	ADDE = 0x83, // A <- A + E
	ADDH = 0x84, // A <- A + H
	ADDL = 0x85, // A <- A + L
	ADDHL = 0x86, // A <- A + [HL]
	ADDA = 0x87, // A <- A + A
	ADDn = 0xC6, // A <- A + n
	ADDHL_BC = 0x09, // HL <- HL + BC
	ADDHL_DE = 0x19, // HL <- HL + DE
	ADDHL_HL = 0x29, // HL <- HL + HL
	ADDHL_SP = 0x39, // HL <- HL + SP
	ADDSP_n = 0xE8, // SP <- SP + n


					/* AND operations */
	ANDB = 0xA0, // A <- A & B
	ANDC = 0xA1, // A <- A & C
	ANDD = 0xA2, // A <- A & D
	ANDE = 0xA3, // A <- A & E
	ANDH = 0xA4, // A <- A & H
	ANDL = 0xA5, // A <- A & L
	ANDHL = 0xA6, // A <- A & [HL]
	ANDA = 0xA7, // A <- A & A
	ANDN = 0xE6, // A < A & n

				 /* Bit test operations */
	BIT_0B = 0xCB40, // Test bit 0 of register B
	BIT_0C = 0xCB41, // Test bit 0 of register C
	BIT_0D = 0xCB42, // Test bit 0 of register D
	BIT_0E = 0xCB43, // Test bit 0 of register E
	BIT_0H = 0xCB44, // Test bit 0 of register H
	BIT_0L = 0xCB45, // Test bit 0 of register L
	BIT_0HL = 0xCB46, // Test bit 0 of byte HL
	BIT_0A = 0xCB47, // Test bit 0 of register A

	BIT_1B = 0xCB48, // Test bit 1 of register B
	BIT_1C = 0xCB49, // Test bit 1 of register C
	BIT_1D = 0xCB4A, // Test bit 1 of register D
	BIT_1E = 0xCB4B, // Test bit 1 of register E
	BIT_1H = 0xCB4C, // Test bit 1 of register H
	BIT_1L = 0xCB4D, // Test bit 1 of register L
	BIT_1HL = 0xCB4E, // Test bit 1 of byte HL
	BIT_1A = 0xCB4F, // Test bit 1 of register A

	BIT_2B = 0xCB50, // Test bit 2 of register B
	BIT_2C = 0xCB51, // Test bit 2 of register C
	BIT_2D = 0xCB52, // Test bit 2 of register D
	BIT_2E = 0xCB53, // Test bit 2 of register E
	BIT_2H = 0xCB54, // Test bit 2 of register H
	BIT_2L = 0xCB55, // Test bit 2 of register L
	BIT_2HL = 0xCB56, // Test bit 2 of byte HL
	BIT_2A = 0xCB57, // Test bit 2 of register A

	BIT_3B = 0xCB58, // Test bit 3 of register B
	BIT_3C = 0xCB59, // Test bit 3 of register C
	BIT_3D = 0xCB5A, // Test bit 3 of register D
	BIT_3E = 0xCB5B, // Test bit 3 of register E
	BIT_3H = 0xCB5C, // Test bit 3 of register H
	BIT_3L = 0xCB5D, // Test bit 3 of register L
	BIT_3HL = 0xCB5E, // Test bit 3 of byte HL
	BIT_3A = 0xCB5F, // Test bit 3 of register A

	BIT_4B = 0xCB60, // Test bit 4 of register B
	BIT_4C = 0xCB61, // Test bit 4 of register C
	BIT_4D = 0xCB62, // Test bit 4 of register D
	BIT_4E = 0xCB63, // Test bit 4 of register E
	BIT_4H = 0xCB64, // Test bit 4 of register H
	BIT_4L = 0xCB65, // Test bit 4 of register L
	BIT_4HL = 0xCB66, // Test bit 4 of byte HL
	BIT_4A = 0xCB67, // Test bit 4 of register A

	BIT_5B = 0xCB68, // Test bit 5 of register B
	BIT_5C = 0xCB69, // Test bit 5 of register C
	BIT_5D = 0xCB6A, // Test bit 5 of register D
	BIT_5E = 0xCB6B, // Test bit 5 of register E
	BIT_5H = 0xCB6C, // Test bit 5 of register H
	BIT_5L = 0xCB6D, // Test bit 5 of register L
	BIT_5HL = 0xCB6E, // Test bit 5 of byte HL
	BIT_5A = 0xCB6F, // Test bit 5 of register A

	BIT_6B = 0xCB70, // Test bit 6 of register B
	BIT_6C = 0xCB71, // Test bit 6 of register C
	BIT_6D = 0xCB72, // Test bit 6 of register D
	BIT_6E = 0xCB73, // Test bit 6 of register E
	BIT_6H = 0xCB74, // Test bit 6 of register H
	BIT_6L = 0xCB75, // Test bit 6 of register L
	BIT_6HL = 0xCB76, // Test bit 6 of byte HL
	BIT_6A = 0xCB77, // Test bit 6 of register A

	BIT_7B = 0xCB78, // Test bit 7 of register B
	BIT_7C = 0xCB79, // Test bit 7 of register C
	BIT_7D = 0xCB7A, // Test bit 7 of register D
	BIT_7E = 0xCB7B, // Test bit 7 of register E
	BIT_7H = 0xCB7C, // Test bit 7 of register H
	BIT_7L = 0xCB7D, // Test bit 7 of register L
	BIT_7HL = 0xCB7E, // Test bit 7 of byte HL
	BIT_7A = 0xCB7F, // Test bit 7 of register A


	/* Call operations */
	CALLNZ_nn = 0xC4, // If (f & 0x80 == 0x00) Push PC onto stack then PC <- [PC + 2, PC + 1]
	CALLZ_nn = 0xCC, // If (f & 0x80 == 0x01) Push PC onto stack then PC <- [PC + 2, PC + 1]
	CALL = 0xCD, // Push PC onto stack then PC <- [PC + 2, PC + 1]
	CALLNC_nn = 0xD4, // If (f & 0x10 == 0x00) Push PC onto stack then PC <- [PC + 2, PC + 1]
	CALLC_nn = 0xDC, // If (f & 0x10 == 0x10) Push PC onto stack then PC <- [PC + 2, PC + 1]


	/* Compliment carry flag */
	CCF = 0x3F, // Invert carry bit in flag register


	/* Compare operations */
	CPB = 0xB8, // Compare A with B
	CPC = 0xB9, // Compare A with C
	CPD = 0xBA, // Compare A with D
	CPE = 0xBB, // Compare A with E
	CPH = 0xBC, // Compare A with H
	CPL = 0xBD, // Compare A with L
	CPHL = 0xBE, // Compare A with [HL]
	CPA = 0xBF, // Compare A with A
	CPn = 0xFE, // Compare A with n


	/* Compliment register A */
	CPRA = 0x2F, // Invert content of register A


	/* Decimal adjust register A */
	DAA = 0x27,


	/* Decrement operations */
	DECB = 0x05, // decrement register B
	DECBC = 0x0B, // decrement register pair BC
	DECC = 0x0D, // decrement register C
	DECD = 0x15, // decrement register D
	DECDE = 0x1B, // decrement register pair DE
	DECE = 0x1D, // decrement register E
	DECH = 0x25, // decrement register H
	DECHL = 0x2B, // decrement register pair HL
	DECL = 0x2D, // decrement register L
	DECmHL = 0x35, // decrement [HL]
	DECSP = 0x3B, // decrement stack pointer
	DECA = 0x3D, // decrement register A


	/* Enable and disable interrupts */
	DI = 0xF3,
	EI = 0xFB,


	/* Halt processor */
	HALT = 0x76,


	/* Increment operations */
	INCBC = 0x03, // increment register pair BC
	INCB = 0x04, // increment register B
	INCC = 0x0C, // increment register C
	INCDE = 0x13, // increment register pair DE
	INCD = 0x14, // increment register D
	INCE = 0x1C, // increment register E
	INCHL = 0x23, // increment register pair HL
	INCH = 0x24, // increment register H
	INCL = 0x2C, // incrememnt register L
	INCSP = 0x33, // increment stack pointer
	INCmHL = 0x34, // increment [HL]
	INCA = 0x3C, // increment register A


	/* Jump operations */
	JPNZ_nn = 0xC2, // // If (f & 0x80 == 0x00) then PC <- [PC + 2, PC + 1]
	JP_nn = 0xC3, // PC <- [PC + 2, PC + 1]
	JPZ_nn = 0xCA, // If (f & 0x80 == 0x01) then PC <- [PC + 2, PC + 1]
	JPNC_nn = 0xD2, // If (f & 0x10 == 0x00) then PC <- [PC + 2, PC + 1]
	JPC_nn = 0xDA, // If (f & 0x10 == 0x01) then PC <- [PC + 2, PC + 1]
	JPHL = 0xE9, // PC <- [HL]


	/* Jump relative operations */
	JR_n = 0x18, // Jump relatively by signed 16-bit immediate
	JRNZ_n = 0x20, // Jump if last result wasn't zero
	JRZ_n = 0x28, // Jump if last result was zero
	JRNC_n = 0x30, // Jump if last result didn't produce a carry
	JRC_n = 0x38, // Jump if last result produced a carry


	/* Load operations */
	LDBC_nn = 0x01, // Load immediate into BC
	LDBC_A = 0x02, // Save A to [BC]
	LDB_n = 0x06, // Load immediate into B
	LDnn_SP = 0x08, // Save SP to [nn]
	LDA_BC = 0x0A, // Load [BC] into A
	LDC_n = 0x0E, // Load immediate into C
	LDDE_nn = 0x11, // Load immediate into DE
	LDDE_A = 0x12, // Save A to [DE]
	LDD_n = 0x16, // Load immediate into D
	LDA_DE = 0x1A, // Load [DE] into A
	LDE_n = 0x1E, // Load immediate into E
	LDHL_nn = 0x21, // Load immediate into HL
	LDIHL_A = 0x22, // Save A to [HL] and increment HL
	LDH_n = 0x26, // Load immediate into H
	LDIA_HL = 0x2A, // Load [HL] to A and increment HL
	LDL_n = 0x2E, // Load immediate into L
	LDSP_nn = 0x31, // Load immediate into SP
	LDDHL_A = 0x32, // Save A to [HL] and decrement HL
	LDHL_n = 0x36, // Save immediate to [HL]
	LDDA_HL = 0x3A, // Load [HL] into A and decrement HL
	LDA_n = 0x3E, // Load immediate into A
	LDB_B = 0x40, // Copy B to B
	LDB_C = 0x41, // Copy C to B
	LDB_D = 0x42, // Copy D to B
	LDB_E = 0x43, // Copy E to B
	LDB_H = 0x44, // Copy H to B
	LDB_L = 0x45, // Copy L to B
	LDB_HL = 0x46, // Copy [HL] to B
	LDB_A = 0x47, // Copy A to B
	LDC_B = 0x48, // Copy B to C
	LDC_C = 0x49, // Copy C to C
	LDC_D = 0x4A, // Copy D to C
	LDC_E = 0x4B, // Copy E to C
	LDC_H = 0x4C, // Copy H to C
	LDC_L = 0x4D, // Copy L to C
	LDC_HL = 0x4E, // Copy [HL] to C
	LDC_A = 0x4F, // Copy A to C
	LDD_B = 0x50, // Copy B to D
	LDD_C = 0x51, // Copy C to D
	LDD_D = 0x52, // Copy D to D
	LDD_E = 0x53, // Copy E to D
	LDD_H = 0x54, // Copy H to D
	LDD_L = 0x55, // Copy L to D
	LDD_HL = 0x56, // Copy [HL] to D
	LDD_A = 0x57, // Copy A to D
	LDE_B = 0x58, // Copy B to E
	LDE_C = 0x59, // Copy C to E
	LDE_D = 0x5A, // Copy D to E
	LDE_E = 0x5B, // Copy E to E
	LDE_H = 0x5C, // Copy H to E
	LDE_L = 0x5D, // Copy L to E
	LDE_HL = 0x5E, // Copy [HL] to E
	LDE_A = 0x5F, // Copy A to E
	LDH_B = 0x60, // Copy B to H
	LDH_C = 0x61, // Copy C to H
	LDH_D = 0x62, // Copy D to H
	LDH_E = 0x63, // Copy E to H
	LDH_H = 0x64, // Copy H to H
	LDH_L = 0x65, // Copy L to H
	LDH_HL = 0x66, // Copy H to [HL]
	LDH_A = 0x67, // Copy A to H
	LDL_B = 0x68, // Copy B to L
	LDL_C = 0x69, // Copy C to L
	LDL_D = 0x6A, // Copy D to L
	LDL_E = 0x6B, // Copy E to L
	LDL_H = 0x6C, // Copy H to L
	LDL_L = 0x6D, // Copy L to L
	LDL_HL = 0x6E, // Copy L to [HL]
	LDL_A = 0x6F, // Copy L to A
	LDHL_B = 0x70, // Save B to [HL]
	LDHL_C = 0x71, // Save C to [HL]
	LDHL_D = 0x72, // Save D to [HL]
	LDHL_E = 0x73, // Save E to [HL]
	LDHL_H = 0x74, // Save H to [HL]
	LDHL_L = 0x75, // Save L to [HL]
	LDHL_A = 0x77, // Save A to [HL]
	LDA_B = 0x78, // Copy B to A
	LDA_C = 0x79, // Copy C to A
	LDA_D = 0x7A, // Copy D to A
	LDA_E = 0x7B, // Copy E to A
	LDA_H = 0x7C, // Copy H to A
	LDA_L = 0x7D, // Copy L to A
	LDA_HL = 0x7E, // Copy [HL] to A
	LDA_A = 0x7F, // Copy A to A
	LDhn_A = 0xE0, // Save A to [FF00 + n]
	LDhC_A = 0xE2, // Save A to [FF00 + C]
	LDnn_A = 0xEA, // Save A to [nn]
	LDhA_n = 0xF0, // Load [FF00 + n] to A
	LDhA_C = 0xF2, // Load [FF00 + C] to A
	LDHL_SPd = 0xF8, // Add signed immediate byte to SP and write result in HL
	LDSP_HL = 0xF9, // Copy HL to SP
	LDA_nn = 0xFA, // Save [nn] to A


				   /* No operation */
	NOP = 0x00,


	/* OR operations */
	ORB = 0xB0, // A <- A or B
	ORC = 0xB1, // A <- A or C
	ORD = 0xB2, // A <- A or D
	ORE = 0xB3, // A <- A or E
	ORH = 0xB4, // A <- A or H
	ORL = 0xB5, // A <- A or L
	ORHL = 0xB6, // A <- A or [HL]
	ORA = 0xB7, // A <- A or A
	ORn = 0xF6, // A <- A or n


	/* Pop operations */
	POP_BC = 0xC1, // Pop stack to BC
	POP_DE = 0xD1, // Pop stack to DE
	POP_HL = 0xE1, // Pop stack to HL
	POP_AF = 0xF1, // Pop stack to AF


	/* Push operations */
	PUSH_BC = 0xC5, // Push BC onto stack
	PUSH_DE = 0xD5, // Push DE onto stack
	PUSH_HL = 0xE5, // Push HL onto stack
	PUSH_AF = 0xF5, // Push AF onto stack


					/* Return operations */
	RETNZ = 0xC0, // Return if last result was not zero
	RETZ = 0xC8, // Return if last result was zero
	RET = 0xC9, // Return unconditionally
	RETNC = 0xD0, // Return if last result didn't produced a carry
	RETC = 0xD8, // Return if last result produced a carry
	RETI = 0xD9, // Enable interrupts and return


				 /* Rotate operations */
	RLC_B = 0xCB00, // Rotate B left with carry
	RLC_C = 0xCB01, // Rotate C left with carry
	RLC_D = 0xCB02, // Rotate D left with carry
	RLC_E = 0xCB03, // Rotate E left with carry
	RLC_H = 0xCB04, // Rotate H left with carry
	RLC_L = 0xCB05, // Rotate L left with carry
	RLC_HL = 0xCB06, // Rotate [HL] left with carry
	RLC_A = 0xCB07, // Rotate A left with carry
	RLC_A_2B = 0x07, // Rotate A left with carry (2 byte instruction)

	RRC_B = 0xCB08, // Rotate B right with carry
	RRC_C = 0xCB09, // Rotate C right with carry
	RRC_D = 0xCB0A, // Rotate D right with carry
	RRC_E = 0xCB0B, // Rotate E right with carry
	RRC_H = 0xCB0C, // Rotate H right with carry
	RRC_L = 0xCB0D, // Rotate L right with carry
	RRC_HL = 0xCB0E, // Rotate [HL] right with carry
	RRC_A_2B = 0xCB0F, // Rotate A right with carry (2 byte instruction)
	RRC_A = 0x0F, // Rotate A right with carry

	RLB = 0xCB10, // Rotate B left
	RLC = 0xCB11, // Rotate C left
	RLD = 0xCB12, // Rotate D left
	RLE = 0xCB13, // Rotate E left
	RLH = 0xCB14, // Rotate H left
	RLL = 0xCB15, // Rotate L left
	RLHL = 0xCB16, // Rotate [HL] left
	RLA = 0xCB17, // Rotate A left
	RLA_2B = 0x17, // Rotate A left

	RRB = 0xCB18, // Rotate B right
	RRC = 0xCB19, // Rotate C right
	RRD = 0xCB1A, // Rotate D right
	RRE = 0xCB1B, // Rotate E right
	RRH = 0xCB1C, // Rotate H right
	RRL = 0xCB1D, // Rotate L right
	RRHL = 0xCB1E, // Rotate HL right
	RRA = 0xCB1F, // Rotate A right
	RRA_2B = 0x1F, // Rotate A right (2 byte instruction)


				   /* Shift operations */
	SLAB = 0xCB20, // Shift B left preserving sign
	SLAC = 0xCB21, // Shift C left preserving sign
	SLAD = 0xCB22, // Shift D left preserving sign
	SLAE = 0xCB23, // Shift E left preserving sign
	SLAH = 0xCB24, // Shift H left preserving sign
	SLAL = 0xCB25, // Shift L left preserving sign
	SLAHL = 0xCB26, // Shift HL left preserving sign
	SLAA = 0xCB27, // Shift A left preserving sign

	SRAB = 0xCB28, // Shift B right preserving sign
	SRAC = 0xCB29, // Shift C right preserving sign
	SRAD = 0xCB2A, // Shift D right preserving sign
	SRAE = 0xCB2B, // Shift E right preserving sign
	SRAH = 0xCB2C, // Shift H right preserving sign
	SRAL = 0xCB2D, // Shift L right preserving sign
	SRAHL = 0xCB2E, // Shift HL right preserving sign
	SRAA = 0xCB2F, // Shift A right preserving sign

	SRLB = 0xCB38, // Shift B right
	SRLC = 0xCB39, // Shift C right
	SRLD = 0xCB3A, // Shift D right
	SRLE = 0xCB3B, // Shift E right
	SRLH = 0xCB3C, // Shift H right
	SRLL = 0xCB3D, // Shift L right
	SRLHL = 0xCB3E, // Shift HL right
	SRLA = 0xCB3F, // Shift A right


				   /* Set carry flag */
	SCF = 0x37, // Set carry flag


				/* Stop processor */
	STOP = 0x10,


	/* Call fixed address routines */
	RST0 = 0xC7, // Call routine at address 0000
	RST10 = 0xD7, // Call routine at address 0010
	RST20 = 0xE7, // Call routine at address 0020
	RST30 = 0xF7, // Call routine at address 0030
	RST8 = 0xCF, // Call routine at address 0008
	RST18 = 0xDF, // Call routine at address 0018
	RST28 = 0xEF, // Call routine at address 0028
	RST38 = 0xFF, // Call routine at address 0038


				  /* Subtract operations */
	SBCA_B = 0x98, // A <- A - B - carry
	SBCA_C = 0x99, // A <- A - C - carry
	SBCA_D = 0x9A, // A <- A - D - carry
	SBCA_E = 0x9B, // A <- A - E - carry
	SBCA_H = 0x9C, // A <- A - H - carry
	SBCA_L = 0x9D, // A <- A - L - carry
	SBCA_HL = 0x9E, // A <- A - [HL] - carry
	SBCA_A = 0x9F, // A <- A - A - carry
	SBCA_n = 0xDE, // A <- A - n - carry

	SUBA_B = 0x90, // A <- A - B
	SUBA_C = 0x91, // A <- A - C
	SUBA_D = 0x92, // A <- A - D
	SUBA_E = 0x93, // A <- A - E
	SUBA_H = 0x94, // A <- A - H
	SUBA_L = 0x95, // A <- A - L
	SUBA_HL = 0x96, // A <- A - HL
	SUBA_A = 0x97, // A <- A - A
	SUBA_n = 0xD6, // A <- A - n


				   /* Swap operations */
	SWAPB = 0xCB30, // Swap nybbles in B
	SWAPC = 0xCB31, // Swap nybbles in C
	SWAPD = 0xCB32, // Swap nybbles in D
	SWAPE = 0xCB33, // Swap nybbles in E
	SWAPH = 0xCB34, // Swap nybbles in H
	SWAPL = 0xCB35, // Swap nybbles in L
	SWAPHL = 0xCB36, // Swap nybbles in HL
	SWAPA = 0xCB37, // Swap nybbles in A


					/* Reset operations */
	RES_0B = 0xCB80, // Clear bit 0 of register B
	RES_0C = 0xCB81, // Clear bit 0 of register C
	RES_0D = 0xCB82, // Clear bit 0 of register D
	RES_0E = 0xCB83, // Clear bit 0 of register E
	RES_0H = 0xCB84, // Clear bit 0 of register H
	RES_0L = 0xCB85, // Clear bit 0 of register L
	RES_0HL = 0xCB86, // Clear bit 0 of byte HL
	RES_0A = 0xCB87, // Clear bit 0 of register A

	RES_1B = 0xCB88, // Clear bit 1 of register B
	RES_1C = 0xCB89, // Clear bit 1 of register C
	RES_1D = 0xCB8A, // Clear bit 1 of register D
	RES_1E = 0xCB8B, // Clear bit 1 of register E
	RES_1H = 0xCB8C, // Clear bit 1 of register H
	RES_1L = 0xCB8D, // Clear bit 1 of register L
	RES_1HL = 0xCB8E, // Clear bit 1 of byte HL
	RES_1A = 0xCB8F, // Clear bit 1 of register A

	RES_2B = 0xCB90, // Clear bit 2 of register B
	RES_2C = 0xCB91, // Clear bit 2 of register C
	RES_2D = 0xCB92, // Clear bit 2 of register D
	RES_2E = 0xCB93, // Clear bit 2 of register E
	RES_2H = 0xCB94, // Clear bit 2 of register H
	RES_2L = 0xCB95, // Clear bit 2 of register L
	RES_2HL = 0xCB96, // Clear bit 2 of byte HL
	RES_2A = 0xCB97, // Clear bit 2 of register A

	RES_3B = 0xCB98, // Clear bit 3 of register B
	RES_3C = 0xCB99, // Clear bit 3 of register C
	RES_3D = 0xCB9A, // Clear bit 3 of register D
	RES_3E = 0xCB9B, // Clear bit 3 of register E
	RES_3H = 0xCB9C, // Clear bit 3 of register H
	RES_3L = 0xCB9D, // Clear bit 3 of register L
	RES_3HL = 0xCB9E, // Clear bit 3 of byte HL
	RES_3A = 0xCB9F, // Clear bit 3 of register A

	RES_4B = 0xCBA0, // Clear bit 4 of register B
	RES_4C = 0xCBA1, // Clear bit 4 of register C
	RES_4D = 0xCBA2, // Clear bit 4 of register D
	RES_4E = 0xCBA3, // Clear bit 4 of register E
	RES_4H = 0xCBA4, // Clear bit 4 of register H
	RES_4L = 0xCBA5, // Clear bit 4 of register L
	RES_4HL = 0xCBA6, // Clear bit 4 of byte HL
	RES_4A = 0xCBA7, // Clear bit 4 of register A

	RES_5B = 0xCBA8, // Clear bit 5 of register B
	RES_5C = 0xCBA9, // Clear bit 5 of register C
	RES_5D = 0xCBAA, // Clear bit 5 of register D
	RES_5E = 0xCBAB, // Clear bit 5 of register E
	RES_5H = 0xCBAC, // Clear bit 5 of register H
	RES_5L = 0xCBAD, // Clear bit 5 of register L
	RES_5HL = 0xCBAE, // Clear bit 5 of byte HL
	RES_5A = 0xCBAF, // Clear bit 5 of register A

	RES_6B = 0xCBB0, // Clear bit 6 of register B
	RES_6C = 0xCBB1, // Clear bit 6 of register C
	RES_6D = 0xCBB2, // Clear bit 6 of register D
	RES_6E = 0xCBB3, // Clear bit 6 of register E
	RES_6H = 0xCBB4, // Clear bit 6 of register H
	RES_6L = 0xCBB5, // Clear bit 6 of register L
	RES_6HL = 0xCBB6, // Clear bit 6 of byte HL
	RES_6A = 0xCBB7, // Clear bit 6 of register A

	RES_7B = 0xCBB8, // Clear bit 7 of register B
	RES_7C = 0xCBB9, // Clear bit 7 of register C
	RES_7D = 0xCBBA, // Clear bit 7 of register D
	RES_7E = 0xCBBB, // Clear bit 7 of register E
	RES_7H = 0xCBBC, // Clear bit 7 of register H
	RES_7L = 0xCBBD, // Clear bit 7 of register L
	RES_7HL = 0xCBBE, // Clear bit 7 of byte HL
	RES_7A = 0xCBBF, // Clear bit 7 of register A


	/* Set operations */
	SET_0B = 0xCBC0, // Set bit 0 of B
	SET_0C = 0xCBC1, // Set bit 0 of C
	SET_0D = 0xCBC2, // Set bit 0 of D
	SET_0E = 0xCBC3, // Set bit 0 of E
	SET_0H = 0xCBC4, // Set bit 0 of H
	SET_0L = 0xCBC5, // Set bit 0 of L
	SET_0HL = 0xCBC6, // Set bit 0 of [HL]
	SET_0A = 0xCBC7, // Set bit 0 of A

	SET_1B = 0xCBC8, // Set bit 1 of B
	SET_1C = 0xCBC9, // Set bit 1 of C
	SET_1D = 0xCBCA, // Set bit 1 of D
	SET_1E = 0xCBCB, // Set bit 1 of E
	SET_1H = 0xCBCC, // Set bit 1 of H
	SET_1L = 0xCBCD, // Set bit 1 of L
	SET_1HL = 0xCBCE, // Set bit 1 of [HL]
	SET_1A = 0xCBCF, // Set bit 1 of A

	SET_2B = 0xCBD0, // Set bit 2 of B
	SET_2C = 0xCBD1, // Set bit 2 of C
	SET_2D = 0xCBD2, // Set bit 2 of D
	SET_2E = 0xCBD3, // Set bit 2 of E
	SET_2H = 0xCBD4, // Set bit 2 of H
	SET_2L = 0xCBD5, // Set bit 2 of L
	SET_2HL = 0xCBD6, // Set bit 2 of [HL]
	SET_2A = 0xCBD7, // Set bit 2 of A

	SET_3B = 0xCBD8, // Set bit 3 of B
	SET_3C = 0xCBD9, // Set bit 3 of C
	SET_3D = 0xCBDA, // Set bit 3 of D
	SET_3E = 0xCBDB, // Set bit 3 of E
	SET_3H = 0xCBDC, // Set bit 3 of H
	SET_3L = 0xCBDD, // Set bit 3 of L
	SET_3HL = 0xCBDE, // Set bit 3 of [HL]
	SET_3A = 0xCBDF, // Set bit 3 of A

	SET_4B = 0xCBE0, // Set bit 4 of B
	SET_4C = 0xCBE1, // Set bit 4 of C
	SET_4D = 0xCBE2, // Set bit 4 of D
	SET_4E = 0xCBE3, // Set bit 4 of E
	SET_4H = 0xCBE4, // Set bit 4 of H
	SET_4L = 0xCBE5, // Set bit 4 of L
	SET_4HL = 0xCBE6, // Set bit 4 of [HL]
	SET_4A = 0xCBE7, // Set bit 4 of A

	SET_5B = 0xCBE8, // Set bit 5 of B
	SET_5C = 0xCBE9, // Set bit 5 of C
	SET_5D = 0xCBEA, // Set bit 5 of D
	SET_5E = 0xCBEB, // Set bit 5 of E
	SET_5H = 0xCBEC, // Set bit 5 of H
	SET_5L = 0xCBED, // Set bit 5 of L
	SET_5HL = 0xCBEE, // Set bit 5 of [HL]
	SET_5A = 0xCBEF, // Set bit 5 of A

	SET_6B = 0xCBF0, // Set bit 6 of B
	SET_6C = 0xCBF1, // Set bit 6 of C
	SET_6D = 0xCBF2, // Set bit 6 of D
	SET_6E = 0xCBF3, // Set bit 6 of E
	SET_6H = 0xCBF4, // Set bit 6 of H
	SET_6L = 0xCBF5, // Set bit 6 of L
	SET_6HL = 0xCBF6, // Set bit 6 of [HL]
	SET_6A = 0xCBF7, // Set bit 6 of A

	SET_7B = 0xCBF8, // Set bit 7 of B
	SET_7C = 0xCBF9, // Set bit 7 of C
	SET_7D = 0xCBFA, // Set bit 7 of D
	SET_7E = 0xCBFB, // Set bit 7 of E
	SET_7H = 0xCBFC, // Set bit 7 of H
	SET_7L = 0xCBFD, // Set bit 7 of L
	SET_7HL = 0xCBFE, // Set bit 7 of [HL]
	SET_7A = 0xCBFF, // Set bit 7 of A


	/* XOR operations */
	XORB = 0xA8, // A <- A ^ B
	XORC = 0xA9, // A <- A ^ C
	XORD = 0xAA, // A <- A ^ D
	XORE = 0xAB, // A <- A ^ E
	XORH = 0xAC, // A <- A ^ H
	XORL = 0xAD, // A <- A ^ L
	XORHL = 0xAE, // A <- A ^ [HL]
	XORA = 0xAF, // A <- A ^ A
	XORn = 0xEE, // A <- A ^n
};

int GetInstructionImmSize(OpCode inst);
