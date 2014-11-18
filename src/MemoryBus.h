#pragma once

#include "Base.h"
#include "Memory.h"

class Cpu;
class Ppu;
class Cartridge;
class CpuInternalRam;

class CpuMemoryBus
{
public:
	CpuMemoryBus();
	void Initialize(Cpu& cpu, Ppu& ppu, Cartridge& cartridge, CpuInternalRam& cpuInternalRam);

	uint8 Read(uint16 cpuAddress);
	void Write(uint16 cpuAddress, uint8 value);

private:
	Cpu* m_cpu;
	Ppu* m_ppu;
	Cartridge* m_cartridge;
	CpuInternalRam* m_cpuInternalRam;
};

class PpuMemoryBus
{
public:
	PpuMemoryBus();
	void Initialize(Ppu& ppu, Cartridge& cartridge);

	uint8 Read(uint16 ppuAddress);
	void Write(uint16 ppuAddress, uint8 value);

private:
	Ppu* m_ppu;
	Cartridge* m_cartridge;
};
