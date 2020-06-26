
#pragma once

#include <cstdint>

class IMappedComponent
{
public:
	virtual uint8_t ReadByte(uint16_t addr) = 0;
	virtual void WriteByte(uint16_t addr, uint8_t value) = 0;
};