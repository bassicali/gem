
#include <cassert>

#include "Core/Instruction.h"

using namespace std;

#define NUM_OPCODES 500

OpCodeIndex::OpCodeIndex()
{
	list = new OpCodeInfo[NUM_OPCODES]
	{
#define OPCODE(op,e,mn) OpCodeInfo(uint16_t(op),e,mn),
#include "OpcodeTable.inl"
#undef OPCODE
	};

	opcodeToIdxMap = new short[0x200];
	usedIndices = new short[512];
	memset(usedIndices, 0, sizeof(short) * 512);

	for (int i = 0; i < NUM_OPCODES; i++)
	{
		uint16_t opcode = list[i].OpCode;
		if ((opcode & 0xCB00) == 0xCB00)
		{
			opcode = 0x100 | (opcode & 0xFF);
		}
		assert(opcode < 0x200);
		opcodeToIdxMap[opcode] = i;
		usedIndices[opcode] = 1;
	}
}

OpCodeIndex& OpCodeIndex::Get()
{
	static OpCodeIndex instance;
	return instance;
}

OpCodeIndex::~OpCodeIndex()
{
	if (list)
	{
		delete[] list;
	}
}

int GetIndex(uint16_t opcode)
{
	if ((opcode & 0xCB00) == 0xCB00)
	{
		return 0x100 | (opcode & 0xFF);
	}
	else
	{
		return opcode & 0xFF;
	}
}

bool OpCodeIndex::Lookup(uint16_t opcode, OpCodeInfo& info)
{
	int idx = GetIndex(opcode);

	if (idx >= 0x200 || usedIndices[idx] == 0)
		return false;

	idx = opcodeToIdxMap[idx];
	info = list[idx];
	return true;
}

bool OpCodeIndex::Contains(uint16_t opcode)
{
	int idx = GetIndex(opcode);
	return usedIndices[idx] == 1;
}

OpCodeInfo& OpCodeIndex::operator[](int opcode)
{
	uint16_t inst;
	if ((opcode & 0xCB00) == 0xCB00)
	{
		inst = 0x1 | (opcode & 0xFF);
	}
	else
	{
		inst = opcode & 0xFF;
	}

	int idx = opcodeToIdxMap[opcode];
	return list[idx];
}
	
int OpCodeIndex::GetImmSize(uint16_t inst)
{
	switch (static_cast<OpCode>(inst))
	{
	case OpCode::ADCn:
	case OpCode::ADDn:
	case OpCode::ADDSP_n:
	case OpCode::ANDN:
	case OpCode::ORn:
	case OpCode::XORn:
	case OpCode::CPn:
	case OpCode::JRNZ_n:
	case OpCode::JRZ_n:
	case OpCode::JRNC_n:
	case OpCode::JRC_n:
	case OpCode::JR_n:
	case OpCode::LDHL_n:
	case OpCode::LDhn_A:
	case OpCode::LDhA_n:
	case OpCode::LDhA_C:
	case OpCode::LDHL_SPd:
	case OpCode::SBCA_n:
	case OpCode::SUBA_n:
	case OpCode::LDB_n:
	case OpCode::LDC_n:
	case OpCode::LDD_n:
	case OpCode::LDE_n:
	case OpCode::LDH_n:
	case OpCode::LDL_n:
	case OpCode::LDA_n:
		return 1;

	case OpCode::CALLZ_nn:
	case OpCode::CALLNZ_nn:
	case OpCode::CALLC_nn:
	case OpCode::CALLNC_nn:
	case OpCode::CALL:
	case OpCode::JPNZ_nn:
	case OpCode::JPZ_nn:
	case OpCode::JPNC_nn:
	case OpCode::JPC_nn:
	case OpCode::JP_nn:
	case OpCode::LDBC_nn:
	case OpCode::LDDE_nn:
	case OpCode::LDHL_nn:
	case OpCode::LDSP_nn:
	case OpCode::LDnn_A:
	case OpCode::LDA_nn:
	case OpCode::LDnn_SP:
		return 2;

	default:
		return 0;
	}
}