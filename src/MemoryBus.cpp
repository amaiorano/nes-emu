#include "MemoryBus.h"
#include "Cpu.h"
#include "Ppu.h"
#include "Apu.h"
#include "Cartridge.h"
#include "CpuInternalRam.h"
#include "MemoryMap.h"

CpuMemoryBus::CpuMemoryBus()
	: m_ppu(nullptr)
	, m_cartridge(nullptr)
	, m_cpuInternalRam(nullptr)
{
}

void CpuMemoryBus::Initialize(Cpu& cpu, Ppu& ppu, Apu& apu, Cartridge& cartridge, CpuInternalRam& cpuInternalRam)
{
	m_cpu = &cpu;
	m_ppu = &ppu;
	m_apu = &apu;
	m_cartridge = &cartridge;
	m_cpuInternalRam = &cpuInternalRam;
}

uint8 CpuMemoryBus::Read(uint16 cpuAddress)
{
	uint8 apuResult = 0;
	//@TODO: Implement pAPU registers
	if (cpuAddress == CpuMemory::kApuFrameCounter)
	{
		//@TODO: Conflict: address $4017 is used by both controller port 2 and APU
		apuResult = m_apu->HandleCpuRead(cpuAddress);
	}

	if (cpuAddress >= CpuMemory::kExpansionRomBase)
	{
		return m_cartridge->HandleCpuRead(cpuAddress);
	}
	else if (cpuAddress >= CpuMemory::kCpuRegistersBase)
	{
		return m_cpu->HandleCpuRead(cpuAddress) | apuResult;
	}
	else if (cpuAddress >= CpuMemory::kPpuRegistersBase)
	{
		return m_ppu->HandleCpuRead(cpuAddress);
	}

	return m_cpuInternalRam->HandleCpuRead(cpuAddress);
}

void CpuMemoryBus::Write(uint16 cpuAddress, uint8 value)
{
	switch (cpuAddress)
	{
	case CpuMemory::kApuPulse1ChannelA:
	case CpuMemory::kApuPulse1ChannelB:
	case CpuMemory::kApuPulse1ChannelC:
	case CpuMemory::kApuPulse1ChannelD:
	case CpuMemory::kApuPulse2ChannelA:
	case CpuMemory::kApuPulse2ChannelB:
	case CpuMemory::kApuPulse2ChannelC:
	case CpuMemory::kApuPulse2ChannelD:
	case CpuMemory::kApuTriangleChannelA:
	case CpuMemory::kApuTriangleChannelB:
	case CpuMemory::kApuTriangleChannelC:
	case CpuMemory::kApuNoiseChannelA:
	case CpuMemory::kApuNoiseChannelB:
	case CpuMemory::kApuNoiseChannelC:
	case CpuMemory::kApuDMCChannelA:
	case CpuMemory::kApuDMCChannelB:
	case CpuMemory::kApuDMCChannelC:
	case CpuMemory::kApuDMCChannelD:
	case CpuMemory::kApuControlStatus:
	case CpuMemory::kApuFrameCounter:
		//@TODO: Conflict: address $4017 is used by both controller port 2 and APU
		m_apu->HandleCpuWrite(cpuAddress, value);
	}

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
