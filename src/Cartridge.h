#pragma once
#include "Base.h"
#include "Memory.h"
#include "Rom.h"
#include "Mapper.h"
#include <memory>

class Cartridge
{
public:
	void Initialize();
	RomHeader LoadRom(const char* file);

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

	ScreenArrangement GetScreenArrangement() { return m_screenArrangement; }

private:
	uint8& AccessPrgMem(uint16 cpuAddress);
	uint8& AccessChrMem(uint16 ppuAddress);
	uint16 Cartridge::MapCpuToSram(uint16 cpuAddress);
	
	ScreenArrangement m_screenArrangement;
	std::shared_ptr<Mapper> m_mapperHolder;
	Mapper* m_mapper;

	// Set arbitrarily large max number of banks
	static const size_t kMaxPrgBanks = 128;
	static const size_t kMaxChrBanks = 128;

	typedef Memory<FixedSizeStorage<kPrgBankSize>> PrgBankMemory;
	typedef Memory<FixedSizeStorage<kChrBankSize>> ChrBankMemory;

	std::array<PrgBankMemory, kMaxPrgBanks> m_prgBanks;
	std::array<ChrBankMemory, kMaxChrBanks> m_chrBanks;	

	Memory<FixedSizeStorage<KB(8)>> m_sram;
};
