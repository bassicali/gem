
#pragma once

#include <cstdint>

// An interace to share a small subset of the MMU with the GPU class. We can't do this directly
// because MMU depends on GPU
class IMMU
{
public:
	virtual uint8_t ReadByte(uint16_t addr) = 0;
	virtual void WriteByte(uint16_t addr, uint8_t value) = 0;
};