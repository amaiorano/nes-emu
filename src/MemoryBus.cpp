#include "MemoryBus.h"
#include "Cpu.h"
#include "Ppu.h"
#include "Cartridge.h"
#include "CpuInternalRam.h"
#include "MemoryMap.h"

CpuMemoryBus::CpuMemoryBus()
	: m_ppu(nullptr)
	, m_cartridge(nullptr)
	, m_cpuInternalRam(nullptr)
{
}

void CpuMemoryBus::Initialize(Cpu& cpu, Ppu& ppu, Cartridge& cartridge, CpuInternalRam& cpuInternalRam)
{
	m_cpu = &cpu;
	m_ppu = &ppu;
	m_cartridge = &cartridge;
	m_cpuInternalRam = &cpuInternalRam;
}

uint8 CpuMemoryBus::Read(uint16 cpuAddress)
{
	if (cpuAddress >= CpuMemory::kExpansionRomBase)
	{
		return m_cartridge->HandleCpuRead(cpuAddress);
	}
	else if (cpuAddress >= CpuMemory::kCpuRegistersBase)
	{
		return m_cpu->HandleCpuRead(cpuAddress);
	}
	else if (cpuAddress >= CpuMemory::kPpuRegistersBase)
	{
		return m_ppu->HandleCpuRead(cpuAddress);
	}

	return m_cpuInternalRam->HandleCpuRead(cpuAddress);
}

void CpuMemoryBus::Write(uint16 cpuAddress, uint8 value)
{
	if (cpuAddress >= CpuMemory::kExpansionRomBase)
	{
		m_cartridge->HandleCpuWrite(cpuAddress, value);
		return;
	}
	else if (cpuAddress >= CpuMemory::kCpuRegistersBase)
	{
		m_cpu->HandleCpuWrite(cpuAddress, value);
		return;
	}
	else if (cpuAddress >= CpuMemory::kPpuRegistersBase)
	{
		m_ppu->HandleCpuWrite(cpuAddress, value);
		return;
	}

	m_cpuInternalRam->HandleCpuWrite(cpuAddress, value);
}


PpuMemoryBus::PpuMemoryBus()
	: m_ppu(nullptr)
	, m_cartridge(nullptr)
{
}

void PpuMemoryBus::Initialize(Ppu& ppu, Cartridge& cartridge)
{
	m_ppu = &ppu;
	m_cartridge = &cartridge;
}

uint8 PpuMemoryBus::Read(uint16 ppuAddress)
{
	ppuAddress %= PpuMemory::kPpuMemorySize; // Handle mirroring above 16K to 64K

	if (ppuAddress >= PpuMemory::kVRamBase)
	{
		return m_ppu->HandlePpuRead(ppuAddress);
	}

	return m_cartridge->HandlePpuRead(ppuAddress);
}

void PpuMemoryBus::Write(uint16 ppuAddress, uint8 value)
{
	ppuAddress %= PpuMemory::kPpuMemorySize; // Handle mirroring above 16K to 64K

	if (ppuAddress >= PpuMemory::kVRamBase)
	{
		return m_ppu->HandlePpuWrite(ppuAddress, value);
	}

	return m_cartridge->HandlePpuWrite(ppuAddress, value);
}
