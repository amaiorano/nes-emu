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
	for (;;)
	{
		//@TODO: For now, run a bunch of CPU instructions for every PPU update. Eventually need to make it cycle based.
		for (size_t i = 0; i < 1; ++i)
		{
			m_cpu.Run();
		}
		m_ppu.Run();
	}
}

