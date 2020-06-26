#include <string.h>
#include <exception>
#include <sstream>
#include <iomanip>

#include "DecodedInstruction.h"

#define IMM1 setw(2) << unsigned(imm1)
#define IMM2 setw(2) << unsigned(imm2)

using namespace std;

DecodedInstruction::DecodedInstruction() :
	is_instruction(false), instruction(Instruction(0)),
	address(0), data(nullptr), size(0)
{
	
}

DecodedInstruction::DecodedInstruction(const DecodedInstruction& other) :
	is_instruction(false), instruction(Instruction(0)),
	address(0), data(nullptr), size(0)
{
	DeepCopy(other);
}

DecodedInstruction& DecodedInstruction::operator=(const DecodedInstruction& other)
{
	DeepCopy(other);
	return *this;
}

DecodedInstruction::~DecodedInstruction()
{
	delete[] data;
	data = nullptr;
}

void DecodedInstruction::DeepCopy(const DecodedInstruction& other)
{
	if (data != nullptr)
	{
		delete[] data;
	}

	is_instruction = other.is_instruction;
	instruction = other.instruction;
	size = other.size;
	address = other.address;
	mnemonic = other.mnemonic;
	as_string = other.as_string;
	if (size > 0)
	{
		data = new uint8_t[size];
		memcpy_s(data, size, other.data, size);
	}
}

DecodedInstruction::DecodedInstruction(uint8_t* data, unsigned int size) : 
	is_instruction(false), instruction(Instruction(0))
{
	if (data == nullptr)
	{
		throw exception("Can't initialize diassembly from null data.");
	}
	
	this->data = data;
	this->size = size;
	mnemonic = TranslateToMnemonic(*this);
	updateAsString();
}

DecodedInstruction::DecodedInstruction(uint16_t addr, Instruction inst) :
	is_instruction(true), instruction(inst), address(addr)
{
	// The CBxx instructions are always 2 bytes long
	if (inst && 0xCB00 == 0xCB00)
	{
		size = 2;
		data = new uint8_t[size];
		data[0] = 0xCB;
		data[1] = inst && 0xFF;
	}
	else
	{
		// It's assumed that this diassembly only consists of an opcode which can only be 1 byte long. 
		// For single-byte instructions that take 1 or 2 bytes of immediate data, the other ctors are used.
		size = 1;
		data = new uint8_t[size];
		data[0] = inst && 0xFF;
	}

	mnemonic = TranslateToMnemonic(*this);
	updateAsString();
}

DecodedInstruction::DecodedInstruction(uint16_t addr, Instruction inst, uint8_t b0) :
	is_instruction(true), instruction(inst), size(2), address(addr)
{
	data = new uint8_t[size];
	data[0] = inst && 0xFF;
	data[1] = b0;
	mnemonic = TranslateToMnemonic(*this);
	updateAsString();
}

DecodedInstruction::DecodedInstruction(uint16_t addr, Instruction inst, uint8_t b0, uint8_t b1) :
	is_instruction(true), instruction(inst), size(3), address(addr)
{
	data = new uint8_t[size];
	data[0] = inst && 0xFF;
	data[1] = b0;
	data[2] = b1;
	mnemonic = TranslateToMnemonic(*this);
	updateAsString();
}


void DecodedInstruction::updateAsString()
{
	char buff[50];
	if (size == 1)
		sprintf_s(buff, "%04x %02x %12c %s", address, data[0], ' ', mnemonic.c_str());
	else if (size == 2)
		sprintf_s(buff, "%04x %02x %02x %9c %s", address, data[0], data[1], ' ', mnemonic.c_str());
	else if (size == 3)
		sprintf_s(buff, "%04x %02x %02x %02x %6c %s", address, data[0], data[1], data[2], ' ', mnemonic.c_str());

	as_string = buff;
}

uint32_t DecodedInstruction::GetDataAsLeftPaddedInt()
{
	uint32_t concat_data = 0;
	if (size > 0) concat_data = data[0];
	if (size > 1) concat_data = (concat_data << 8) | data[1];
	if (size > 2) concat_data = (concat_data << 8) | data[2];
	return concat_data;
}

string DecodedInstruction::GetMnemonic()
{
	if (mnemonic.empty())
	{
		mnemonic = TranslateToMnemonic(*this);
	}

	return mnemonic;
}

string DecodedInstruction::TranslateToMnemonic(DecodedInstruction& dasm)
{
	stringstream mnemonic;
	mnemonic << hex << setfill('0');
	uint8_t imm1 = 0, imm2 = 0;
	if (dasm.size > 0) imm1 = dasm.data[1];
	if (dasm.size > 1) imm2 = dasm.data[2];

	switch (dasm.instruction)
	{
		case ADCB: mnemonic << "adc b"; break;
		case ADCC: mnemonic << "adc c"; break;
		case ADCD: mnemonic << "adc d"; break;
		case ADCE: mnemonic << "adc e"; break;
		case ADCH: mnemonic << "adc h"; break;
		case ADCL: mnemonic << "adc l"; break;
		case ADCHL: mnemonic << "adc (hl)"; break;
		case ADCA: mnemonic << "adc a"; break;
		case ADCn: mnemonic << "adc " << IMM1; break;
		case ADDB: mnemonic << "add b"; break;
		case ADDC: mnemonic << "add c"; break;
		case ADDD: mnemonic << "add d"; break;
		case ADDE: mnemonic << "add e"; break;
		case ADDH: mnemonic << "add h"; break;
		case ADDL: mnemonic << "add l"; break;
		case ADDHL: mnemonic << "add (hl)"; break;
		case ADDA: mnemonic << "add a"; break;
		case ADDn: mnemonic << "add " << IMM1; break;
		case ADDHL_BC: mnemonic << "add hl,bc"; break;
		case ADDHL_DE: mnemonic << "add hl,de"; break;
		case ADDHL_HL: mnemonic << "add hl,hl"; break;
		case ADDHL_SP: mnemonic << "add hl,sp"; break;
		case ADDSP_n: mnemonic << "add sp"; break;
		case ANDB: mnemonic << "and b"; break;
		case ANDC: mnemonic << "and c"; break;
		case ANDD: mnemonic << "and d"; break;
		case ANDE: mnemonic << "and e"; break;
		case ANDH: mnemonic << "and h"; break;
		case ANDL: mnemonic << "and l"; break;
		case ANDHL: mnemonic << "and (hl)"; break;
		case ANDA: mnemonic << "and a"; break;
		case ANDN: mnemonic << "and " << IMM1; break;
		case BIT_0B: mnemonic << "bit 0,b"; break;
		case BIT_0C: mnemonic << "bit 0,c"; break;
		case BIT_0D: mnemonic << "bit 0,d"; break;
		case BIT_0E: mnemonic << "bit 0,e"; break;
		case BIT_0H: mnemonic << "bit 0,h"; break;
		case BIT_0L: mnemonic << "bit 0,l"; break;
		case BIT_0HL: mnemonic << "bit 0,(hl)"; break;
		case BIT_0A: mnemonic << "bit 0,a"; break;
		case BIT_1B: mnemonic << "bit 1,(b)"; break;
		case BIT_1C: mnemonic << "bit 1,(c)"; break;
		case BIT_1D: mnemonic << "bit 1,(d)"; break;
		case BIT_1E: mnemonic << "bit 1,(e)"; break;
		case BIT_1H: mnemonic << "bit 1,(h)"; break;
		case BIT_1L: mnemonic << "bit 1,(l)"; break;
		case BIT_1HL: mnemonic << "bit 1,(hl)"; break;
		case BIT_1A: mnemonic << "bit 1,(a)"; break;
		case BIT_2B: mnemonic << "bit 2,(b)"; break;
		case BIT_2C: mnemonic << "bit 2,(c)"; break;
		case BIT_2D: mnemonic << "bit 2,(d)"; break;
		case BIT_2E: mnemonic << "bit 2,(e)"; break;
		case BIT_2H: mnemonic << "bit 2,(h)"; break;
		case BIT_2L: mnemonic << "bit 2,(l)"; break;
		case BIT_2HL: mnemonic << "bit 2,(hl)"; break;
		case BIT_2A: mnemonic << "bit 2,(a)"; break;
		case BIT_3B: mnemonic << "bit 3,(b)"; break;
		case BIT_3C: mnemonic << "bit 3,(c)"; break;
		case BIT_3D: mnemonic << "bit 3,(d)"; break;
		case BIT_3E: mnemonic << "bit 3,(e)"; break;
		case BIT_3H: mnemonic << "bit 3,(h)"; break;
		case BIT_3L: mnemonic << "bit 3,(l)"; break;
		case BIT_3HL: mnemonic << "bit 3,(hl)"; break;
		case BIT_3A: mnemonic << "bit 3,(a)"; break;
		case BIT_4B: mnemonic << "bit 4,(b)"; break;
		case BIT_4C: mnemonic << "bit 4,(c)"; break;
		case BIT_4D: mnemonic << "bit 4,(d)"; break;
		case BIT_4E: mnemonic << "bit 4,(e)"; break;
		case BIT_4H: mnemonic << "bit 4,(h)"; break;
		case BIT_4L: mnemonic << "bit 4,(l)"; break;
		case BIT_4HL: mnemonic << "bit 4,(hl)"; break;
		case BIT_4A: mnemonic << "bit 4,(a)"; break;
		case BIT_5B: mnemonic << "bit 5,(b)"; break;
		case BIT_5C: mnemonic << "bit 5,(c)"; break;
		case BIT_5D: mnemonic << "bit 5,(d)"; break;
		case BIT_5E: mnemonic << "bit 5,(e)"; break;
		case BIT_5H: mnemonic << "bit 5,(h)"; break;
		case BIT_5L: mnemonic << "bit 5,(l)"; break;
		case BIT_5HL: mnemonic << "bit 5,(hl)"; break;
		case BIT_5A: mnemonic << "bit 5,(a)"; break;
		case BIT_6B: mnemonic << "bit 6,(b)"; break;
		case BIT_6C: mnemonic << "bit 6,(c)"; break;
		case BIT_6D: mnemonic << "bit 6,(d)"; break;
		case BIT_6E: mnemonic << "bit 6,(e)"; break;
		case BIT_6H: mnemonic << "bit 6,(h)"; break;
		case BIT_6L: mnemonic << "bit 6,(l)"; break;
		case BIT_6HL: mnemonic << "bit 6,(hl)"; break;
		case BIT_6A: mnemonic << "bit 6,(a)"; break;
		case BIT_7B: mnemonic << "bit 7,(b)"; break;
		case BIT_7C: mnemonic << "bit 7,(c)"; break;
		case BIT_7D: mnemonic << "bit 7,(d)"; break;
		case BIT_7E: mnemonic << "bit 7,(e)"; break;
		case BIT_7H: mnemonic << "bit 7,(h)"; break;
		case BIT_7L: mnemonic << "bit 7,(l)"; break;
		case BIT_7HL: mnemonic << "bit 7,(hl)"; break;
		case BIT_7A: mnemonic << "bit 7,(a)"; break;
		case CALLNZ_nn: mnemonic << "call nz"; break;
		case CALLZ_nn: mnemonic << "call z," << IMM2 << IMM1; break;
		case CALL: mnemonic << "call "<< IMM2 << IMM1; break;
		case CALLNC_nn: mnemonic << " call nc," << IMM2 << IMM1; break;
		case CALLC_nn: mnemonic << "call c," << IMM2 << IMM1; break;
		case CCF: mnemonic << "ccf"; break;
		case CPB: mnemonic << "cp b"; break;
		case CPC: mnemonic << "cp c"; break;
		case CPD: mnemonic << "cp d"; break;
		case CPE: mnemonic << "cp e"; break;
		case CPH: mnemonic << "cp h"; break;
		case CPL: mnemonic << "cp l"; break;
		case CPHL: mnemonic << "cp (hl)"; break;
		case CPA: mnemonic << "cp a"; break;
		case CPn: mnemonic << "cp " << IMM1; break;
		case CPRA: mnemonic << "cpl a"; break;
		case DAA: mnemonic << "dda"; break;
		case DECB: mnemonic << "dec b"; break;
		case DECBC: mnemonic << "dec bc"; break;
		case DECC: mnemonic << "dec c"; break;
		case DECD: mnemonic << "dec d"; break;
		case DECDE: mnemonic << "dec de"; break;
		case DECE: mnemonic << "dec e"; break;
		case DECH: mnemonic << "dec h"; break;
		case DECHL: mnemonic << "dec hl"; break;
		case DECL: mnemonic << "dec l"; break;
		case DECmHL: mnemonic << "dec (hl)"; break;
		case DECSP: mnemonic << "dec sp"; break;
		case DECA: mnemonic << "dec a"; break;
		case DI: mnemonic << "di"; break;
		case EI: mnemonic << "ei"; break;
		case HALT: mnemonic << "halt"; break;
		case INCBC: mnemonic << "inc bc"; break;
		case INCB: mnemonic << "inc b"; break;
		case INCC: mnemonic << "inc c"; break;
		case INCDE: mnemonic << "inc de"; break;
		case INCD: mnemonic << "inc d"; break;
		case INCE: mnemonic << "inc e"; break;
		case INCHL: mnemonic << "inc hl"; break;
		case INCH: mnemonic << "inc h"; break;
		case INCL: mnemonic << "inc l"; break;
		case INCSP: mnemonic << "inc sp"; break;
		case INCmHL: mnemonic << "inc (hl)"; break;
		case INCA: mnemonic << "inc a"; break;
		case JPNZ_nn: mnemonic << "jp nz," << IMM2 << IMM1; break;
		case JP_nn: mnemonic << "jp " << IMM2 << IMM1; break;
		case JPZ_nn: mnemonic << "jp z," << IMM2 << IMM1; break;
		case JPNC_nn: mnemonic << "jp nc," << IMM2 << IMM1; break;
		case JPC_nn: mnemonic << "jp c," << IMM2 << IMM1; break;
		case JPHL: mnemonic << "jp (hl)"; break;
		case JR_n: mnemonic << "jr " << IMM1; break;
		case JRNZ_n: mnemonic << "jr nz," << IMM2 << IMM1; break;
		case JRZ_n: mnemonic << "jr z," << IMM2 << IMM1; break;
		case JRNC_n: mnemonic << "jr nc," << IMM2 << IMM1; break;
		case JRC_n: mnemonic << "jr c," << IMM2 << IMM1; break;
		case LDBC_nn: mnemonic << "ld bc,(" << IMM2 << IMM1 << ")"; break;
		case LDBC_A: mnemonic << "ld bc,a"; break;
		case LDB_n: mnemonic << "ld b," << IMM1; break;
		case LDnn_SP: mnemonic << "ld (" << IMM2 << IMM1 << "),sp"; break;
		case LDA_BC: mnemonic << "ld a,(bc)"; break;
		case LDC_n: mnemonic << "ld c," << IMM1; break;
		case LDDE_nn: mnemonic << "ld de,(" << IMM2 << IMM1 << ")"; break;
		case LDDE_A: mnemonic << "ld (de),a"; break;
		case LDD_n: mnemonic << "ld d," << IMM1; break;
		case LDA_DE: mnemonic << "ld a,(" << IMM2 << IMM1 << ")"; break;
		case LDE_n: mnemonic << "ld e," << IMM1; break;
		case LDHL_nn: mnemonic << "ld hl," << IMM2 << IMM1; break;
		case LDIHL_A: mnemonic << "ldi (hl),a"; break;
		case LDH_n: mnemonic << "ld h," << IMM1; break;
		case LDIA_HL: mnemonic << "ldi a,(hl)"; break;
		case LDL_n: mnemonic << "ld l," << IMM1; break;
		case LDSP_nn: mnemonic << "ld sp," << IMM2 << IMM1; break;
		case LDDHL_A: mnemonic << "ldd (hl),a"; break;
		case LDHL_n: mnemonic << "ld (hl)," << IMM1; break;
		case LDDA_HL: mnemonic << "ldd a,(hl)"; break;
		case LDA_n: mnemonic << "ld a," << IMM1; break;
		case LDB_B: mnemonic << "ld b,b"; break;
		case LDB_C: mnemonic << "ld b,c"; break;
		case LDB_D: mnemonic << "ld b,d"; break;
		case LDB_E: mnemonic << "ld b,e"; break;
		case LDB_H: mnemonic << "ld b,h"; break;
		case LDB_L: mnemonic << "ld b,l"; break;
		case LDB_HL: mnemonic << "ld b,(hl)"; break;
		case LDB_A: mnemonic << "ld b,a"; break;
		case LDC_B: mnemonic << "ld c,b"; break;
		case LDC_C: mnemonic << "ld c,c"; break;
		case LDC_D: mnemonic << "ld c,d"; break;
		case LDC_E: mnemonic << "ld c,e"; break;
		case LDC_H: mnemonic << "ld c,h"; break;
		case LDC_L: mnemonic << "ld c,l"; break;
		case LDC_HL: mnemonic << "ld c,(hl)"; break;
		case LDC_A: mnemonic << "ld c,a"; break;
		case LDD_B: mnemonic << "ld d,b"; break;
		case LDD_C: mnemonic << "ld d,c"; break;
		case LDD_D: mnemonic << "ld d,d"; break;
		case LDD_E: mnemonic << "ld d,e"; break;
		case LDD_H: mnemonic << "ld d,h"; break;
		case LDD_L: mnemonic << "ld d,l"; break;
		case LDD_HL: mnemonic << "ld d,(hl)"; break;
		case LDD_A: mnemonic << "ld d,a"; break;
		case LDE_B: mnemonic << "ld e,b"; break;
		case LDE_C: mnemonic << "ld e,c"; break;
		case LDE_D: mnemonic << "ld e,d"; break;
		case LDE_E: mnemonic << "ld e,e"; break;
		case LDE_H: mnemonic << "ld e,h"; break;
		case LDE_L: mnemonic << "ld e,l"; break;
		case LDE_HL: mnemonic << "ld e,(hl)"; break;
		case LDE_A: mnemonic << "ld e,a"; break;
		case LDH_B: mnemonic << "ld h,b"; break;
		case LDH_C: mnemonic << "ld h,c"; break;
		case LDH_D: mnemonic << "ld h,d"; break;
		case LDH_E: mnemonic << "ld h,e"; break;
		case LDH_H: mnemonic << "ld h,h"; break;
		case LDH_L: mnemonic << "ld h,l"; break;
		case LDH_HL: mnemonic << "ld h,(hl)"; break;
		case LDH_A: mnemonic << "ld h,a"; break;
		case LDL_B: mnemonic << "ld l,b"; break;
		case LDL_C: mnemonic << "ld l,c"; break;
		case LDL_D: mnemonic << "ld l,d"; break;
		case LDL_E: mnemonic << "ld l,e"; break;
		case LDL_H: mnemonic << "ld l,h"; break;
		case LDL_L: mnemonic << "ld l,l"; break;
		case LDL_HL: mnemonic << "ld l,(hl)"; break;
		case LDL_A: mnemonic << "ld l,a"; break;
		case LDHL_B: mnemonic << "ld (hl),b"; break;
		case LDHL_C: mnemonic << "ld (hl),c"; break;
		case LDHL_D: mnemonic << "ld (hl),d"; break;
		case LDHL_E: mnemonic << "ld (hl),e"; break;
		case LDHL_H: mnemonic << "ld (hl),h"; break;
		case LDHL_L: mnemonic << "ld (hl),l"; break;
		case LDHL_A: mnemonic << "ld (hl),a"; break;
		case LDA_B: mnemonic << "ld a,b"; break;
		case LDA_C: mnemonic << "ld a,c"; break;
		case LDA_D: mnemonic << "ld a,d"; break;
		case LDA_E: mnemonic << "ld a,e"; break;
		case LDA_H: mnemonic << "ld a,h"; break;
		case LDA_L: mnemonic << "ld a,l"; break;
		case LDA_HL: mnemonic << "ld a,(hl)"; break;
		case LDA_A: mnemonic << "ld a,a"; break;
		case LDhn_A: mnemonic << "ld (ff00+" << IMM1 << "),a"; break;
		case LDhC_A: mnemonic << "ld (ff00+c),a"; break;
		case LDnn_A: mnemonic << "ld (" << IMM2 << IMM1 << "),a"; break;
		case LDhA_n: mnemonic << "ld a,(ff00+a)" << IMM1; break;
		case LDhA_C: mnemonic << "ld a,(ff00+c)"; break;
		case LDHL_SPd: mnemonic << "ld hl,sp+" << IMM1; break;
		case LDSP_HL: mnemonic << "ld sp,hl"; break;
		case LDA_nn: mnemonic << "ld a,(" << IMM2 << IMM1 << ")"; break;
		case NOP: mnemonic << "nop"; break;
		case ORB: mnemonic << "or b"; break;
		case ORC: mnemonic << "or c"; break;
		case ORD: mnemonic << "or d"; break;
		case ORE: mnemonic << "or e"; break;
		case ORH: mnemonic << "or h"; break;
		case ORL: mnemonic << "or l"; break;
		case ORHL: mnemonic << "or (hl)"; break;
		case ORA: mnemonic << "or a"; break;
		case ORn: mnemonic << "or " << IMM1; break;
		case POP_BC: mnemonic << "pop bc"; break;
		case POP_DE: mnemonic << "pop de"; break;
		case POP_HL: mnemonic << "pop hl"; break;
		case POP_AF: mnemonic << "pop af"; break;
		case PUSH_BC: mnemonic << "push bc"; break;
		case PUSH_DE: mnemonic << "push de"; break;
		case PUSH_HL: mnemonic << "push hl"; break;
		case PUSH_AF: mnemonic << "push af"; break;
		case RETNZ: mnemonic << "ret nz," << IMM2 << IMM1; break;
		case RETZ: mnemonic << "ret z," << IMM2 << IMM1; break;
		case RET: mnemonic << "ret"; break;
		case RETNC: mnemonic << "ret nc," << IMM2 << IMM1; break;
		case RETC: mnemonic << "ret c," << IMM2 << IMM1; break;
		case RETI: mnemonic << "reti"; break;
		case RLC_B: mnemonic << "rlc b"; break;
		case RLC_C: mnemonic << "rlc c"; break;
		case RLC_D: mnemonic << "rlc d"; break;
		case RLC_E: mnemonic << "rlc e"; break;
		case RLC_H: mnemonic << "rlc h"; break;
		case RLC_L: mnemonic << "rlc l"; break;
		case RLC_HL: mnemonic << "rlc (hl)"; break;
		case RLC_A: mnemonic << "rlc a"; break;
		case RLC_A_2B: mnemonic << "rlc a *"; break;
		case RRC_B: mnemonic << "rrc b"; break;
		case RRC_C: mnemonic << "rrc c"; break;
		case RRC_D: mnemonic << "rrc d"; break;
		case RRC_E: mnemonic << "rrc e"; break;
		case RRC_H: mnemonic << "rrc h"; break;
		case RRC_L: mnemonic << "rrc l"; break;
		case RRC_HL: mnemonic << "rrc (hl)"; break;
		case RRC_A_2B: mnemonic << "rrc a *"; break;
		case RRC_A: mnemonic << "rrc a"; break;
		case RLB: mnemonic << "rl b"; break;
		case RLC: mnemonic << "rl c"; break;
		case RLD: mnemonic << "rl d"; break;
		case RLE: mnemonic << "rl e"; break;
		case RLH: mnemonic << "rl h"; break;
		case RLL: mnemonic << "rl l"; break;
		case RLHL: mnemonic << "rl (hl)"; break;
		case RLA: mnemonic << "rl a"; break;
		case RLA_2B: mnemonic << "rla *"; break;
		case RRB: mnemonic << "rr b"; break;
		case RRC: mnemonic << "rr c"; break;
		case RRD: mnemonic << "rr d"; break;
		case RRE: mnemonic << "rr e"; break;
		case RRH: mnemonic << "rr h"; break;
		case RRL: mnemonic << "rr l"; break;
		case RRHL: mnemonic << "rr (hl)"; break;
		case RRA: mnemonic << "rr a"; break;
		case RRA_2B: mnemonic << "rra *"; break;
		case SLAB: mnemonic << "sla b"; break;
		case SLAC: mnemonic << "sla c"; break;
		case SLAD: mnemonic << "sla d"; break;
		case SLAE: mnemonic << "sla e"; break;
		case SLAH: mnemonic << "sla h"; break;
		case SLAL: mnemonic << "sla l"; break;
		case SLAHL: mnemonic << "sla (hl)"; break;
		case SLAA: mnemonic << "sla a"; break;
		case SRAB: mnemonic << "rl b"; break;
		case SRAC: mnemonic << "rl c"; break;
		case SRAD: mnemonic << "rl d"; break;
		case SRAE: mnemonic << "rl e"; break;
		case SRAH: mnemonic << "rl h"; break;
		case SRAL: mnemonic << "rl l"; break;
		case SRAHL: mnemonic << "rl (hl)"; break;
		case SRAA: mnemonic << "rl a"; break;
		case SRLB: mnemonic << "rl b"; break;
		case SRLC: mnemonic << "rl c"; break;
		case SRLD: mnemonic << "rl d"; break;
		case SRLE: mnemonic << "rl e"; break;
		case SRLH: mnemonic << "rl h"; break;
		case SRLL: mnemonic << "rl l"; break;
		case SRLHL: mnemonic << "rl (hl)"; break;
		case SRLA: mnemonic << "rl a"; break;
		case SCF: mnemonic << "scf"; break;
		case STOP: mnemonic << "stop"; break;
		case RST0: mnemonic << "rst 0"; break;
		case RST10: mnemonic << "rst 10"; break;
		case RST20: mnemonic << "rst 20"; break;
		case RST30: mnemonic << "rst 30"; break;
		case RST8: mnemonic << "rst 8"; break;
		case RST18: mnemonic << "rst 18"; break;
		case RST28: mnemonic << "rst 28"; break;
		case RST38: mnemonic << "rst 38"; break;
		case SBCA_B: mnemonic << "sbc b"; break;
		case SBCA_C: mnemonic << "sbc c"; break;
		case SBCA_D: mnemonic << "sbc d"; break;
		case SBCA_E: mnemonic << "sbc e"; break;
		case SBCA_H: mnemonic << "sbc h"; break;
		case SBCA_L: mnemonic << "sbc l"; break;
		case SBCA_HL: mnemonic << "sbc (hl)"; break;
		case SBCA_A: mnemonic << "sbc a"; break;
		case SBCA_n: mnemonic << "sbc a," << IMM1; break;
		case SUBA_B: mnemonic << "sub b"; break;
		case SUBA_C: mnemonic << "sub c"; break;
		case SUBA_D: mnemonic << "sub d"; break;
		case SUBA_E: mnemonic << "sub e"; break;
		case SUBA_H: mnemonic << "sub h"; break;
		case SUBA_L: mnemonic << "sub l"; break;
		case SUBA_HL: mnemonic << "sub (hl)"; break;
		case SUBA_A: mnemonic << "sub a"; break;
		case SUBA_n: mnemonic << "sub a," << IMM1; break;
		case SWAPB: mnemonic << "swap b"; break;
		case SWAPC: mnemonic << "swap c"; break;
		case SWAPD: mnemonic << "swap d"; break;
		case SWAPE: mnemonic << "swap e"; break;
		case SWAPH: mnemonic << "swap h"; break;
		case SWAPL: mnemonic << "swap l"; break;
		case SWAPHL: mnemonic << "swap (hl)"; break;
		case SWAPA: mnemonic << "swap a"; break;
		case RES_0B: mnemonic << "res 0,(b)"; break;
		case RES_0C: mnemonic << "res 0,(c)"; break;
		case RES_0D: mnemonic << "res 0,(d)"; break;
		case RES_0E: mnemonic << "res 0,(e)"; break;
		case RES_0H: mnemonic << "res 0,(h)"; break;
		case RES_0L: mnemonic << "res 0,(l)"; break;
		case RES_0HL: mnemonic << "res 0,(hl)"; break;
		case RES_0A: mnemonic << "res 0,(a)"; break;
		case RES_1B: mnemonic << "res 1,(b)"; break;
		case RES_1C: mnemonic << "res 1,(c)"; break;
		case RES_1D: mnemonic << "res 1,(d)"; break;
		case RES_1E: mnemonic << "res 1,(e)"; break;
		case RES_1H: mnemonic << "res 1,(h)"; break;
		case RES_1L: mnemonic << "res 1,(l)"; break;
		case RES_1HL: mnemonic << "res 1,(hl)"; break;
		case RES_1A: mnemonic << "res 1,(a)"; break;
		case RES_2B: mnemonic << "res 2,(b)"; break;
		case RES_2C: mnemonic << "res 2,(c)"; break;
		case RES_2D: mnemonic << "res 2,(d)"; break;
		case RES_2E: mnemonic << "res 2,(e)"; break;
		case RES_2H: mnemonic << "res 2,(h)"; break;
		case RES_2L: mnemonic << "res 2,(l)"; break;
		case RES_2HL: mnemonic << "res 2,(hl)"; break;
		case RES_2A: mnemonic << "res 2,(a)"; break;
		case RES_3B: mnemonic << "res 3,(b)"; break;
		case RES_3C: mnemonic << "res 3,(c)"; break;
		case RES_3D: mnemonic << "res 3,(d)"; break;
		case RES_3E: mnemonic << "res 3,(e)"; break;
		case RES_3H: mnemonic << "res 3,(h)"; break;
		case RES_3L: mnemonic << "res 3,(l)"; break;
		case RES_3HL: mnemonic << "res 3,(hl)"; break;
		case RES_3A: mnemonic << "res 3,(a)"; break;
		case RES_4B: mnemonic << "res 4,(b)"; break;
		case RES_4C: mnemonic << "res 4,(c)"; break;
		case RES_4D: mnemonic << "res 4,(d)"; break;
		case RES_4E: mnemonic << "res 4,(e)"; break;
		case RES_4H: mnemonic << "res 4,(h)"; break;
		case RES_4L: mnemonic << "res 4,(l)"; break;
		case RES_4HL: mnemonic << "res 4,(hl)"; break;
		case RES_4A: mnemonic << "res 4,(a)"; break;
		case RES_5B: mnemonic << "res 5,(b)"; break;
		case RES_5C: mnemonic << "res 5,(c)"; break;
		case RES_5D: mnemonic << "res 5,(d)"; break;
		case RES_5E: mnemonic << "res 5,(e)"; break;
		case RES_5H: mnemonic << "res 5,(h)"; break;
		case RES_5L: mnemonic << "res 5,(l)"; break;
		case RES_5HL: mnemonic << "res 5,(hl)"; break;
		case RES_5A: mnemonic << "res 5,(a)"; break;
		case RES_6B: mnemonic << "res 6,(b)"; break;
		case RES_6C: mnemonic << "res 6,(c)"; break;
		case RES_6D: mnemonic << "res 6,(d)"; break;
		case RES_6E: mnemonic << "res 6,(e)"; break;
		case RES_6H: mnemonic << "res 6,(h)"; break;
		case RES_6L: mnemonic << "res 6,(l)"; break;
		case RES_6HL: mnemonic << "res 6,(hl)"; break;
		case RES_6A: mnemonic << "res 6,(a)"; break;
		case RES_7B: mnemonic << "res 7,(b)"; break;
		case RES_7C: mnemonic << "res 7,(c)"; break;
		case RES_7D: mnemonic << "res 7,(d)"; break;
		case RES_7E: mnemonic << "res 7,(e)"; break;
		case RES_7H: mnemonic << "res 7,(h)"; break;
		case RES_7L: mnemonic << "res 7,(l)"; break;
		case RES_7HL: mnemonic << "res 7,(hl)"; break;
		case RES_7A: mnemonic << "res 7,(a)"; break;
		case SET_0B: mnemonic << "set 0,(b)"; break;
		case SET_0C: mnemonic << "set 0,(c)"; break;
		case SET_0D: mnemonic << "set 0,(d)"; break;
		case SET_0E: mnemonic << "set 0,(e)"; break;
		case SET_0H: mnemonic << "set 0,(h)"; break;
		case SET_0L: mnemonic << "set 0,(l)"; break;
		case SET_0HL: mnemonic << "set 0,(hl)"; break;
		case SET_0A: mnemonic << "set 0,(a)"; break;
		case SET_1B: mnemonic << "set 1,(b)"; break;
		case SET_1C: mnemonic << "set 1,(c)"; break;
		case SET_1D: mnemonic << "set 1,(d)"; break;
		case SET_1E: mnemonic << "set 1,(e)"; break;
		case SET_1H: mnemonic << "set 1,(h)"; break;
		case SET_1L: mnemonic << "set 1,(l)"; break;
		case SET_1HL: mnemonic << "set 1,(hl)"; break;
		case SET_1A: mnemonic << "set 1,(a)"; break;
		case SET_2B: mnemonic << "set 2,(b)"; break;
		case SET_2C: mnemonic << "set 2,(c)"; break;
		case SET_2D: mnemonic << "set 2,(d)"; break;
		case SET_2E: mnemonic << "set 2,(e)"; break;
		case SET_2H: mnemonic << "set 2,(h)"; break;
		case SET_2L: mnemonic << "set 2,(l)"; break;
		case SET_2HL: mnemonic << "set 2,(hl)"; break;
		case SET_2A: mnemonic << "set 2,(a)"; break;
		case SET_3B: mnemonic << "set 3,(b)"; break;
		case SET_3C: mnemonic << "set 3,(c)"; break;
		case SET_3D: mnemonic << "set 3,(d)"; break;
		case SET_3E: mnemonic << "set 3,(e)"; break;
		case SET_3H: mnemonic << "set 3,(h)"; break;
		case SET_3L: mnemonic << "set 3,(l)"; break;
		case SET_3HL: mnemonic << "set 3,(hl)"; break;
		case SET_3A: mnemonic << "set 3,(a)"; break;
		case SET_4B: mnemonic << "set 4,(b)"; break;
		case SET_4C: mnemonic << "set 4,(c)"; break;
		case SET_4D: mnemonic << "set 4,(d)"; break;
		case SET_4E: mnemonic << "set 4,(e)"; break;
		case SET_4H: mnemonic << "set 4,(h)"; break;
		case SET_4L: mnemonic << "set 4,(l)"; break;
		case SET_4HL: mnemonic << "set 4,(hl)"; break;
		case SET_4A: mnemonic << "set 4,(a)"; break;
		case SET_5B: mnemonic << "set 5,(b)"; break;
		case SET_5C: mnemonic << "set 5,(c)"; break;
		case SET_5D: mnemonic << "set 5,(d)"; break;
		case SET_5E: mnemonic << "set 5,(e)"; break;
		case SET_5H: mnemonic << "set 5,(h)"; break;
		case SET_5L: mnemonic << "set 5,(l)"; break;
		case SET_5HL: mnemonic << "set 5,(hl)"; break;
		case SET_5A: mnemonic << "set 5,(a)"; break;
		case SET_6B: mnemonic << "set 6,(b)"; break;
		case SET_6C: mnemonic << "set 6,(c)"; break;
		case SET_6D: mnemonic << "set 6,(d)"; break;
		case SET_6E: mnemonic << "set 6,(e)"; break;
		case SET_6H: mnemonic << "set 6,(h)"; break;
		case SET_6L: mnemonic << "set 6,(l)"; break;
		case SET_6HL: mnemonic << "set 6,(hl)"; break;
		case SET_6A: mnemonic << "set 6,(a)"; break;
		case SET_7B: mnemonic << "set 7,(b)"; break;
		case SET_7C: mnemonic << "set 7,(c)"; break;
		case SET_7D: mnemonic << "set 7,(d)"; break;
		case SET_7E: mnemonic << "set 7,(e)"; break;
		case SET_7H: mnemonic << "set 7,(h)"; break;
		case SET_7L: mnemonic << "set 7,(l)"; break;
		case SET_7HL: mnemonic << "set 7,(hl)"; break;
		case SET_7A: mnemonic << "set 7,(a)"; break;
		case XORB: mnemonic << "xor b"; break;
		case XORC: mnemonic << "xor c"; break;
		case XORD: mnemonic << "xor d"; break;
		case XORE: mnemonic << "xor e"; break;
		case XORH: mnemonic << "xor h"; break;
		case XORL: mnemonic << "xor l"; break;
		case XORHL: mnemonic << "xor (hl)"; break;
		case XORA: mnemonic << "xor a"; break;
		case XORn: mnemonic << "xor " << IMM1; break;
		default: mnemonic << "???"; break;
	}

	return mnemonic.str();
}

