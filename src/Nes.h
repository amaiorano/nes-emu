#pragma once

#include "Cpu.h"
#include "Ppu.h"
#include "Cartridge.h"
#include "Memory.h"
#include "CpuInternalRam.h"
#include "MemoryBus.h"

class Nes
{
public:
	void Initialize();
	RomHeader LoadRom(const char* file);
	void Reset();
	void Run();

	void SignalCpuNmi()
	{
		m_cpu.Nmi();
	}

private:
	friend class DebuggerImpl;

	Cpu m_cpu;
	Ppu m_ppu;
	Cartridge m_cartridge;
	CpuInternalRam m_cpuInternalRam;
	CpuMemoryBus m_cpuMemoryBus;
	PpuMemoryBus m_ppuMemoryBus;
};
