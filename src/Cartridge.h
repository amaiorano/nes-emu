#pragma once
#include "Base.h"
#include "Memory.h"
#include "Rom.h"
#include "Mapper.h"
#include <memory>
#include <string>

class Cartridge
{
public:
	void Initialize();
	RomHeader LoadRom(const char* file);

	NameTableMirroring GetNameTableMirroring() const;

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

	void WriteSaveRamFile();
	
private:
	void LoadSaveRamFile();

	uint8& AccessPrgMem(uint16 cpuAddress);
	uint8& AccessChrMem(uint16 ppuAddress);
	uint8& AccessSavMem(uint16 cpuAddress);
	
	std::string m_romDirectory;
	std::string m_romFileNameNoExt;
	std::string m_saveRamPath;

	NameTableMirroring m_cartNameTableMirroring;
	std::shared_ptr<Mapper> m_mapperHolder;
	Mapper* m_mapper;

	// Set arbitrarily large max number of banks
	static const size_t kMaxPrgBanks = 128;
	static const size_t kMaxChrBanks = 128;
	static const size_t kMaxSavBanks = 4;

	typedef Memory<FixedSizeStorage<kPrgBankSize>> PrgBankMemory;
	typedef Memory<FixedSizeStorage<kChrBankSize>> ChrBankMemory;
	typedef Memory<FixedSizeStorage<KB(8)>> SavBankMemory;

	std::array<PrgBankMemory, kMaxPrgBanks> m_prgBanks;
	std::array<ChrBankMemory, kMaxChrBanks> m_chrBanks;
	std::array<SavBankMemory, kMaxSavBanks> m_savBanks;
};
