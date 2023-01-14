#pragma once

#include <cstdint>
#include <string>
#include <fstream>

#include "Core/InterruptController.h"

//#define WRITE_SERIAL_TO_FILE

#define CLOCK_TYPE_EXTERNAL 0
#define CLOCK_TYPE_INTERNAL 1

class SerialController
{
public:
	SerialController();
	~SerialController();
	void WriteByte(uint8_t value);
	uint8_t ReadByte() const;
	uint8_t TxData;
	std::string SavedTxData;
private:
	bool TxStart;
	int ShiftClockType;
	uint8_t RegisterByte;
	#ifdef WRITE_SERIAL_TO_FILE
	std::ofstream fout;
	#endif

	friend class RewindManager;
};
