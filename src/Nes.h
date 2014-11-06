#pragma once

#include "Cpu.h"
#include "Ppu.h"
#include "Memory.h"
#include "Rom.h"

class Nes
{
public:
	void Initialize();
	void LoadRom(const char* file);
	void Reset();
	void Run();

	const RomHeader& GetRomHeader() const { return m_romHeader; }

	void OnCpuMemoryPreRead(uint16 address)
	{
		m_ppu.OnCpuMemoryPreRead(address);
	}

	void OnCpuMemoryPostRead(uint16 address)
	{
		m_ppu.OnCpuMemoryPostRead(address);
	}

	void OnCpuMemoryPostWrite(uint16 address)
	{
		m_ppu.OnCpuMemoryPostWrite(address);
	}

	void SignalCpuNmi()
	{
		m_cpu.Nmi();
	}

private:
	friend class DebuggerImpl;

	RomHeader m_romHeader;
	Cpu m_cpu;
	Ppu m_ppu;
	CpuRam m_cpuRam;
	PpuRam m_ppuRam;
	SpriteRam m_spriteRam;
};
