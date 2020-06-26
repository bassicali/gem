#pragma once

#include <cstdint>

#include "Core/Serial.h"

using namespace std;

SerialController::SerialController() : 
	TxData(0), 
	TxStart(false),
	ShiftClockType(CLOCK_TYPE_INTERNAL)
{
	
	#ifdef WRITE_SERIAL_TO_FILE
	fout.open("serial.txt", ios_base::out | ios_base::trunc);
	#endif
}

SerialController::~SerialController()
{	
	#ifdef WRITE_SERIAL_TO_FILE
	fout.flush();
	fout.close();
	#endif
}

void SerialController::WriteByte(uint8_t value)
{
	// Ignoring bit 1 (clock speed)
	TxStart = (value & 0x80) == 0x80;
	ShiftClockType = (value & 0x1);

	if (TxStart && ShiftClockType == CLOCK_TYPE_INTERNAL)
	{
		SavedTxData += char(TxData);
		#ifdef WRITE_SERIAL_TO_FILE
		fout << char(TxData);
		fout.flush();
		#endif
	}

	RegisterByte = value;
}

uint8_t SerialController::ReadByte() const
{
	return RegisterByte;
}


