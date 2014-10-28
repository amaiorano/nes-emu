#pragma once

#include "Cpu.h"
#include "Ppu.h"
#include "Memory.h"
#include "Rom.h"

class Nes
{
public:
	void LoadRom(const char* file);
	void Reset();
	void Run();

	const RomHeader& GetRomHeader() const { return m_romHeader; }

	void OnCpuMemoryRead(uint16 address)
	{
		m_ppu.OnCpuMemoryRead(address);
	}

	void OnCpuMemoryWrite(uint16 address)
	{
		m_ppu.OnCpuMemoryWrite(address);
	}

private:
	RomHeader m_romHeader;
	Cpu m_cpu;
	Ppu m_ppu;
	CpuRam m_cpuRam;
	PpuRam m_ppuRam;
	SpriteRam m_spriteRam;
};
