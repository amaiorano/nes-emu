#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "Debugger.h"

void Nes::Initialize()
{
	Debugger::Initialize(*this);

	m_cpu.Initialize(m_cpuMemoryBus);
	m_ppu.Initialize(m_ppuMemoryBus, *this);
	m_cartridge.Initialize();
	m_cpuInternalRam.Initialize();
	m_cpuMemoryBus.Initialize(m_ppu, m_cartridge, m_cpuInternalRam);
	m_ppuMemoryBus.Initialize(m_ppu, m_cartridge);
}

RomHeader Nes::LoadRom(const char* file)
{
	return m_cartridge.LoadRom(file);
}

void Nes::Reset()
{
	m_cpu.Reset();
	m_ppu.Reset();
}

void Nes::Run()
{
	int32 numCpuCyclesLeft = 0;

	for (;;)
	{
		uint32 numCpuCyclesToExecute = 0;
		m_ppu.Execute(numCpuCyclesToExecute);

		numCpuCyclesLeft += numCpuCyclesToExecute;

		assert(numCpuCyclesLeft > 0);

		uint32 actualCpuCycles = 0;
		m_cpu.Execute(numCpuCyclesLeft, actualCpuCycles);
		numCpuCyclesLeft -= actualCpuCycles; // Usually negative
	}
}
