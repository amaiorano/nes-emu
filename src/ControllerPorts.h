#pragma once

#include "Base.h"

namespace ControllerButtons
{
	enum Type
	{
		Left,
		Right,
		Up,
		Down,
		A,
		B,
		Select,
		Start,

		Size
	};

	static const char* Names[] =
	{
		"Left",
		"Right",
		"Up",
		"Down",
		"A",
		"B",
		"Select",
		"Start"
	};
	static_assert(ARRAYSIZE(Names) == Size, "Mismatched size");
}

class ControllerPorts
{
public:
	void Initialize();
	void Reset();
	void Serialize(class Serializer& serializer);

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);

private:
	uint16 MapCpuToPorts(uint16 cpuAddress);

	bool m_strobe;
	const static size_t kNumControllers = 2;
	uint8 m_ports[kNumControllers]; // For read only
	uint8 m_readIndex[kNumControllers];
	bool m_lastIsButtonDown[kNumControllers][ControllerButtons::Size];
};
