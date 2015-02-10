#pragma once
#include "Base.h"
#include <memory>

struct PulseChannel;
struct FrameCounter;

class Apu
{
public:
	void Initialize();
	void Execute(uint32 cpuCycles);
	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);

//private:
	bool m_evenFrame;
	std::shared_ptr<FrameCounter> m_frameCounter;
	std::shared_ptr<PulseChannel> m_pulse1;

};
