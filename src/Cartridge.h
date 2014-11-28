#pragma once
#include "Base.h"
#include "Memory.h"
#include "Rom.h"

class Cartridge
{
public:
	void Initialize()
	{
		m_sram.Initialize();
	}

	RomHeader LoadRom(const char* file);

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

private:
	uint16 MapCpuToPrgRom(uint16 cpuAddress);
	uint16 MapCpuToSram(uint16 cpuAddress);
	uint16 MapPpuToChrRom(uint16 ppuAddress);	

	Memory<DynamicSizeStorage> m_prgRom;
	Memory<DynamicSizeStorage> m_chrRom;
	Memory<FixedSizeStorage<KB(8)>> m_sram; // Mapper 0 doesn't support SRAM, but we keep have it anyway for instr_test
};
