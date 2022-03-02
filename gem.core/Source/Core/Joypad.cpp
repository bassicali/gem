#pragma once

#include <memory>
#include <cstdint>

#include "Core/Joypad.h"
#include "Logging.h"

using namespace std;

// All the joypad pins are active low, hence some inverse logic

Joypad::Joypad()
	: keyTypeSelect(JoypadKeyType::None)
	, lowerNibble(0xF)
{
	for (int i = 0; i < 8; i++) 
		keyStates[i] = false;
}

void Joypad::SetInterruptController(std::shared_ptr<InterruptController> ptr)
{
	interrupts = ptr;
}

void Joypad::Press(JoypadKey key)
{
	int key_index = static_cast<int>(key);
	keyStates[key_index] = true;

	bool is_direction_key = key_index >= 2 && key_index <= 5;

	if (keyTypeSelect == JoypadKeyType::Direction && is_direction_key)
		interrupts->JoypadRequested = true;
	else if (keyTypeSelect == JoypadKeyType::Button && !is_direction_key)
		interrupts->JoypadRequested = true;
}

void Joypad::Release(JoypadKey key)
{
	int value = static_cast<int>(key);
	keyStates[value] = false;
}

uint8_t Joypad::ReadByte()
{
	lowerNibble = 0;

	if (keyTypeSelect == JoypadKeyType::Direction)
	{
		lowerNibble = (keyStates[GetKeyInt(JoypadKey::Down)] << 3)
						| (keyStates[GetKeyInt(JoypadKey::Up)] << 2)
						| (keyStates[GetKeyInt(JoypadKey::Left)] << 1)
						| (keyStates[GetKeyInt(JoypadKey::Right)] << 0);
	}
	else if (keyTypeSelect == JoypadKeyType::Button)
	{
		lowerNibble = (keyStates[GetKeyInt(JoypadKey::Start)] << 3)
						| (keyStates[GetKeyInt(JoypadKey::Select)] << 2)
						| (keyStates[GetKeyInt(JoypadKey::B)] << 1)
						| (keyStates[GetKeyInt(JoypadKey::A)] << 0);
	}

	lowerNibble = ~lowerNibble & 0xF;

	uint8_t register_byte = (~static_cast<uint8_t>(keyTypeSelect) & 0xF0) | lowerNibble;
	return register_byte;
}

void Joypad::WriteByte(uint8_t value)
{
	uint8_t masked = value & 0x30;

	if (masked == 0x20 || masked == 0x30) // Bit 4 is active
		keyTypeSelect = JoypadKeyType::Direction;
	else if (masked == 0x10) // Bit 5 is active
		keyTypeSelect = JoypadKeyType::Button;
	else
		keyTypeSelect = JoypadKeyType::None;
}
