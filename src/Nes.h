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

	void SignalCpuNmi() { m_cpu.Nmi(); }
	void SignalCpuIrq() { m_cpu.Irq(); }

	void HACK_OnScanline() { m_cartridge.HACK_OnScanline(); }

	NameTableMirroring GetNameTableMirroring() const { return m_cartridge.GetNameTableMirroring(); }

private:
	friend class DebuggerImpl;

	void ExecuteCpuAndPpuFrame();

	Cpu m_cpu;
	Ppu m_ppu;
	Cartridge m_cartridge;
	CpuInternalRam m_cpuInternalRam;
	CpuMemoryBus m_cpuMemoryBus;
	PpuMemoryBus m_ppuMemoryBus;
};
