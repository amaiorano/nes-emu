#include "ControllerPorts.h"
#include "MemoryMap.h"
#include "Debugger.h"
#include "Input.h"
#include <string>

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

	const char* Names[] =
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

namespace
{
	bool ReadInputDown(uint16 controllerIndex, ControllerButtons::Type button)
	{
		static SDL_Scancode buttonMapping[] =
		{
			SDL_SCANCODE_LEFT,
			SDL_SCANCODE_RIGHT,
			SDL_SCANCODE_UP,
			SDL_SCANCODE_DOWN,
			SDL_SCANCODE_A,
			SDL_SCANCODE_S,
			SDL_SCANCODE_TAB,
			SDL_SCANCODE_RETURN
		};
		static_assert(ARRAYSIZE(buttonMapping) == ControllerButtons::Size, "Mismatched size");

		// For second controller, hold alternate key
		if ((controllerIndex == 1) != Input::AltDown())
			return false;

		const bool isDown = Input::KeyDown(buttonMapping[button]);

		//if (isDown)
		//	printf("%d: KeyDown: %s -> %s\n", controllerIndex, Input::GetScancodeName(buttonMapping[button]), ControllerButtons::Names[button]);

		return isDown;
	}
}

void ControllerPorts::Initialize()
{
	Reset();
}

void ControllerPorts::Reset()
{
	m_strobe = true;
	m_ports[0] = m_ports[1] = 0;
	m_readIndex[0] = m_readIndex[1] = 0;
}

uint8 ControllerPorts::HandleCpuRead(uint16 cpuAddress)
{
	const uint16 controllerIndex = MapCpuToPorts(cpuAddress);
	uint8& port = m_ports[controllerIndex];
	uint8& readIndex = m_readIndex[controllerIndex];

	if (Debugger::IsExecuting())
	{
		return port;
	}

	using namespace ControllerButtons;
	static const ControllerButtons::Type reportOrder[] = { A, B, Select, Start, Up, Down, Left, Right };

	bool isButtonDown = true; // Return 1 after returning state of all buttons
	if (readIndex < ARRAYSIZE(reportOrder))
	{
		const ControllerButtons::Type button = reportOrder[readIndex];
		isButtonDown = ReadInputDown(controllerIndex, button);
	}

	// From http://wiki.nesdev.com/w/index.php/Standard_controller
	//  In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus.
	//  Usually this is the most significant byte of the address of the controller port—0x40. Paperboy relies on this behavior and 
	//  requires that reads from the controller ports return exactly $40 or $41 as appropriate.
	//@TODO: For now, will just hack it to assume this was the last bus value. Eventually could do this correctly in CpuMemoryBus
	const uint8 lastCpuBusValue = 0x40;

	port = lastCpuBusValue | (isButtonDown? 1 : 0);

	// While strobe is off, move to next read button
	if (!m_strobe)
	{
		readIndex = std::min<uint8>(readIndex + 1, ARRAYSIZE(reportOrder));
	}

	return port;
}

void ControllerPorts::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	if (cpuAddress == CpuMemory::kControllerPort1)
	{
		const bool lastStrobe = m_strobe;
		m_strobe = TestBits(value, BIT(0));

		// If strobe set to high, or went from high to low, we reset shift register.
		if (m_strobe || lastStrobe)
		{
			m_readIndex[0] = m_readIndex[1] = 0;
		}
	}
}

uint16 ControllerPorts::MapCpuToPorts(uint16 cpuAddress)
{
	if (cpuAddress == CpuMemory::kControllerPort1)
		return 0;
	else if (cpuAddress == CpuMemory::kControllerPort2)
		return 1;

	assert(false && "Unexpected address");
	return 0;
}
