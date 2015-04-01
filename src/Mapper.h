#pragma once

#include "Base.h"
#include "Rom.h"
#include "Serializer.h"
#include <array>

const size_t kPrgBankCount = 8;
const size_t kPrgBankSize = KB(4);

const size_t kChrBankCount = 8;
const size_t kChrBankSize = KB(1);

const size_t kSavBankCount = 1;
const size_t kSavBankSize = KB(8);

// Mapper is used to map cartridge (physical) memory banks to CPU/PPU (virtual) memory banks.
// A bank is a chunk of memory of fixed size (e.g. 4K for CPU). In the virtual address space of 
// the CPU/PPU, the number of banks are limited, but there may be more physical banks on the cartridge;
// thus the mapper's job is to detect when it needs to remap (switch) certain banks by snooping for
// specific writes on the CPU/PPU memory bus.

class Mapper
{
public:
	// Public interface mostly for Cartridge

	void Initialize(size_t numPrgBanks, size_t numChrBanks, size_t numSavBanks)
	{
		m_nametableMirroring = NameTableMirroring::Undefined;
		m_numPrgBanks = numPrgBanks;
		m_numChrBanks = numChrBanks;
		m_numSavBanks = numSavBanks;
		m_canWritePrgMemory = false;
		m_canWriteChrMemory = false;
		m_canWriteSavMemory = true;

		if (m_numChrBanks == 0)
		{
			m_numChrBanks = 8; // 8K of CHR-RAM
			m_canWriteChrMemory = true;
		}

		// Default init banks to most common mapping
		SetPrgBankIndex32k(0, 0);
		SetChrBankIndex8k(0, 0);
		SetSavBankIndex8k(0, 0);

		PostInitialize();
	}

	virtual const char* MapperName() const = 0;
	virtual void PostInitialize() = 0;
	virtual void Serialize(class Serializer& serializer);
	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value) = 0;

	NameTableMirroring GetNameTableMirroring() const { return m_nametableMirroring; }

	bool CanWritePrgMemory() const { return m_canWritePrgMemory; }
	bool CanWriteChrMemory() const { return m_canWriteChrMemory; }
	bool CanWriteSavMemory() const { return m_canWriteSavMemory; }

	size_t GetMappedPrgBankIndex(size_t cpuBankIndex) { return m_prgBankIndices[cpuBankIndex]; }
	size_t GetMappedChrBankIndex(size_t ppuBankIndex) { return m_chrBankIndices[ppuBankIndex]; }
	size_t GetMappedSavBankIndex(size_t cpuBankIndex) { return m_savBankIndices[cpuBankIndex]; }

	size_t PrgMemorySize() const { return m_numPrgBanks * kPrgBankSize; }
	size_t ChrMemorySize() const { return m_numChrBanks * kChrBankSize; }
	size_t SavMemorySize() const { return m_numSavBanks * kSavBankSize; }

	size_t NumPrgBanks4k() const { return m_numPrgBanks; }
	size_t NumPrgBanks8k() const { return m_numPrgBanks / 2; }
	size_t NumPrgBanks16k() const { return m_numPrgBanks / 4; }
	size_t NumPrgBanks32k() const { return m_numPrgBanks / 8; }

	size_t NumChrBanks1k() const { return m_numChrBanks; }
	size_t NumChrBanks4k() const { return m_numChrBanks / 4; }
	size_t NumChrBanks8k() const { return m_numChrBanks / 8; }

	size_t NumSavBanks8k() const { return m_numSavBanks; }
	
protected:
	// Protected interface for derived Mapper implementations

	void SetNameTableMirroring(NameTableMirroring value) { m_nametableMirroring = value; }

	void SetPrgBankIndex4k(size_t cpuBankIndex, size_t cartBankIndex);
	void SetPrgBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex);
	void SetPrgBankIndex16k(size_t cpuBankIndex, size_t cartBankIndex);
	void SetPrgBankIndex32k(size_t cpuBankIndex, size_t cartBankIndex);

	void SetChrBankIndex1k(size_t ppuBankIndex, size_t cartBankIndex);
	void SetChrBankIndex4k(size_t ppuBankIndex, size_t cartBankIndex);
	void SetChrBankIndex8k(size_t ppuBankIndex, size_t cartBankIndex);

	void SetSavBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex);

	void SetCanWritePrgMemory(bool enabled) { m_canWritePrgMemory = enabled; }
	void SetCanWriteChrMemory(bool enabled) { m_canWriteChrMemory = enabled; }
	void SetCanWriteSavMemory(bool enabled) { m_canWriteSavMemory = enabled; }

private:
	NameTableMirroring m_nametableMirroring;
	size_t m_numPrgBanks;
	size_t m_numChrBanks;
	size_t m_numSavBanks;
	std::array<size_t, kPrgBankCount> m_prgBankIndices;
	std::array<size_t, kChrBankCount> m_chrBankIndices;
	std::array<size_t, kSavBankCount> m_savBankIndices;
	bool m_canWritePrgMemory;
	bool m_canWriteChrMemory;
	bool m_canWriteSavMemory;
};

// Derived Mappers must call Base::Serialize() if overridden
inline void Mapper::Serialize(class Serializer& serializer)
{
	SERIALIZE(m_nametableMirroring);
	SERIALIZE(m_numPrgBanks);
	SERIALIZE(m_numChrBanks);
	SERIALIZE(m_numSavBanks);
	SERIALIZE(m_prgBankIndices);
	SERIALIZE(m_chrBankIndices);
	SERIALIZE(m_savBankIndices);
	SERIALIZE(m_canWritePrgMemory);
	SERIALIZE(m_canWriteChrMemory);
	SERIALIZE(m_canWriteSavMemory);
}

FORCEINLINE void Mapper::SetPrgBankIndex4k(size_t cpuBankIndex, size_t cartBankIndex)
{
	m_prgBankIndices[cpuBankIndex] = cartBankIndex;
}

FORCEINLINE void Mapper::SetPrgBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex)
{
	cpuBankIndex *= 2;
	cartBankIndex *= 2;
	m_prgBankIndices[cpuBankIndex] = cartBankIndex;
	m_prgBankIndices[cpuBankIndex + 1] = cartBankIndex + 1;
}

FORCEINLINE void Mapper::SetSavBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex)
{
	m_savBankIndices[cpuBankIndex] = cartBankIndex;
}

FORCEINLINE void Mapper::SetPrgBankIndex16k(size_t cpuBankIndex, size_t cartBankIndex)
{
	cpuBankIndex *= 4;
	cartBankIndex *= 4;
	m_prgBankIndices[cpuBankIndex] = cartBankIndex;
	m_prgBankIndices[cpuBankIndex + 1] = cartBankIndex + 1;
	m_prgBankIndices[cpuBankIndex + 2] = cartBankIndex + 2;
	m_prgBankIndices[cpuBankIndex + 3] = cartBankIndex + 3;
}

FORCEINLINE void Mapper::SetPrgBankIndex32k(size_t cpuBankIndex, size_t cartBankIndex)
{
	cpuBankIndex *= 8;
	cartBankIndex *= 8;
	m_prgBankIndices[cpuBankIndex] = cartBankIndex;
	m_prgBankIndices[cpuBankIndex + 1] = cartBankIndex + 1;
	m_prgBankIndices[cpuBankIndex + 2] = cartBankIndex + 2;
	m_prgBankIndices[cpuBankIndex + 3] = cartBankIndex + 3;
	m_prgBankIndices[cpuBankIndex + 4] = cartBankIndex + 4;
	m_prgBankIndices[cpuBankIndex + 5] = cartBankIndex + 5;
	m_prgBankIndices[cpuBankIndex + 6] = cartBankIndex + 6;
	m_prgBankIndices[cpuBankIndex + 7] = cartBankIndex + 7;
}

FORCEINLINE void Mapper::SetChrBankIndex1k(size_t ppuBankIndex, size_t cartBankIndex)
{
	m_chrBankIndices[ppuBankIndex] = cartBankIndex;
}

FORCEINLINE void Mapper::SetChrBankIndex4k(size_t ppuBankIndex, size_t cartBankIndex)
{
	ppuBankIndex *= 4;
	cartBankIndex *= 4;
	m_chrBankIndices[ppuBankIndex] = cartBankIndex;
	m_chrBankIndices[ppuBankIndex + 1] = cartBankIndex + 1;
	m_chrBankIndices[ppuBankIndex + 2] = cartBankIndex + 2;
	m_chrBankIndices[ppuBankIndex + 3] = cartBankIndex + 3;
}

FORCEINLINE void Mapper::SetChrBankIndex8k(size_t ppuBankIndex, size_t cartBankIndex)
{
	ppuBankIndex *= 8;
	cartBankIndex *= 8;
	m_chrBankIndices[ppuBankIndex] = cartBankIndex;
	m_chrBankIndices[ppuBankIndex + 1] = cartBankIndex + 1;
	m_chrBankIndices[ppuBankIndex + 2] = cartBankIndex + 2;
	m_chrBankIndices[ppuBankIndex + 3] = cartBankIndex + 3;
	m_chrBankIndices[ppuBankIndex + 4] = cartBankIndex + 4;
	m_chrBankIndices[ppuBankIndex + 5] = cartBankIndex + 5;
	m_chrBankIndices[ppuBankIndex + 6] = cartBankIndex + 6;
	m_chrBankIndices[ppuBankIndex + 7] = cartBankIndex + 7;
}
