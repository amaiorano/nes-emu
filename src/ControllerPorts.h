#pragma once

#include "Base.h"

class ControllerPorts
{
public:
	void Initialize();
	void Reset();

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);

private:
	uint16 MapCpuToPorts(uint16 cpuAddress);

	bool m_strobe;
	const static size_t kNumControllers = 2;
	uint8 m_ports[kNumControllers]; // For read only
	uint8 m_readIndex[kNumControllers];
};
