#pragma once

#include <cstdint>
#include <mutex>

#include "Core/InterruptController.h"

enum class JoypadKeyType
{
	None = 0x00,
	Button = 0x20,
	Direction = 0x10,
};

enum class JoypadKey
{
	A,
	B,
	Up,
	Down,
	Left,
	Right,
	Start,
	Select
};

class Joypad
{
public:
	Joypad();
	uint8_t ReadByte();
	void WriteByte(uint8_t value);
	void Press(JoypadKey key);
	void Release(JoypadKey key);
	void SetInterruptController(std::shared_ptr<InterruptController> ptr);
private:
	const int GetKeyInt(JoypadKey key) const { return static_cast<int>(key); }

	std::shared_ptr<InterruptController> interrupts;

	// This is the upper nibble
	JoypadKeyType keyTypeSelect;

	// Pre-compute the lower nibble that holds the combined key states to save time in ReadByte()
	uint8_t lowerNibble;
	void UpdateLowerNibble();

	// true indicates a key is pressed down
	bool keyStates[8];
};
