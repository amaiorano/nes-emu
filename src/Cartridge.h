#pragma once
#include "Base.h"
#include "Memory.h"
#include "Rom.h"
#include "Mapper.h"
#include <memory>
#include <string>

class Nes;

class Cartridge
{
public:
	void Initialize(Nes& nes);
	void Serialize(class Serializer& serializer);
	
	RomHeader LoadRom(const char* file);
	bool IsRomLoaded() const { return m_mapper != nullptr; }

	NameTableMirroring GetNameTableMirroring() const;

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

	//@TODO: Rename to SerializeSaveRam to mimic SerializeSaveState
	void WriteSaveRamFile(const char* file);
	void LoadSaveRamFile(const char* file);

	void HACK_OnScanline();
	
	size_t GetPrgBankIndex16k(uint16 cpuAddress) const;
	
private:
	uint8& AccessPrgMem(uint16 cpuAddress);
	uint8& AccessChrMem(uint16 ppuAddress);
	uint8& AccessSavMem(uint16 cpuAddress);

	Nes* m_nes;
	
	std::shared_ptr<Mapper> m_mapperHolder;
	Mapper* m_mapper;
	NameTableMirroring m_cartNameTableMirroring;
	bool m_hasSRAM;

	// Set arbitrarily large max number of banks
	static const size_t kMaxPrgBanks = 128;
	static const size_t kMaxChrBanks = 256;
	static const size_t kMaxSavBanks = 4;

	typedef Memory<FixedSizeStorage<kPrgBankSize>> PrgBankMemory;
	typedef Memory<FixedSizeStorage<kChrBankSize>> ChrBankMemory;
	typedef Memory<FixedSizeStorage<KB(8)>> SavBankMemory;

	std::array<PrgBankMemory, kMaxPrgBanks> m_prgBanks;
	std::array<ChrBankMemory, kMaxChrBanks> m_chrBanks;
	std::array<SavBankMemory, kMaxSavBanks> m_savBanks;
};
