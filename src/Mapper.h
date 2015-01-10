#pragma once

#include "Base.h"
#include <array>

const size_t kPrgBankCount = 8;
const size_t kPrgBankSize = KB(4);
const size_t kChrBankCount = 8;
const size_t kChrBankSize = KB(1);

// Mapper is used to map cartridge (physical) memory banks to CPU/PPU (virtual) memory banks.
// A bank is a chunk of memory of fixed size, 4K for CPU and 1K for PPU. In the virtual address
// space of the CPU/PPU, the number of banks are limited to 8 each, but there may be more physical
// banks on the cartridge; thus the mapper's job is to detect when it needs to remap (switch) certain
// banks by snooping for specific writes on the CPU/PPU memory bus.

class Mapper
{
public:
	// Public interface mostly for Cartridge

	void Initialize(size_t numPrgBanks, size_t numChrBanks)
	{
		m_numPrgBanks = numPrgBanks;
		m_numChrBanks = numChrBanks;
		m_canWritePrgMemory = false;
		m_canWriteChrMemory = false;
		PostInitialize();
	}

	virtual const char* MapperName() const = 0;
	virtual void PostInitialize() = 0;
	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value) = 0;

	bool CanWritePrgMemory() const { return m_canWritePrgMemory; }
	bool CanWriteChrMemory() const { return m_canWriteChrMemory; }

	size_t GetMappedPrgBankIndex(size_t cpuBankIndex) { return m_prgBankIndices[cpuBankIndex]; }
	size_t GetMappedChrBankIndex(size_t ppuBankIndex) { return m_chrBankIndices[ppuBankIndex]; }

protected:
	// Protected interface for derived Mapper implementations

	size_t NumPrgBanks4k() const { return m_numPrgBanks; }
	size_t NumPrgBanks8k() const { return m_numPrgBanks / 2; }
	size_t NumPrgBanks16k() const { return m_numPrgBanks / 4; }

	size_t NumChrBanks1k() const { return m_numChrBanks; }
	size_t NumChrBanks8k() const { return m_numChrBanks / 8; }

	void SetPrgBankIndex4k(size_t cpuBankIndex, size_t cartBankIndex);
	void SetPrgBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex);
	void SetPrgBankIndex16k(size_t cpuBankIndex, size_t cartBankIndex);

	void SetChrBankIndex1k(size_t ppuBankIndex, size_t cartBankIndex);
	void SetChrBankIndex8k(size_t ppuBankIndex, size_t cartBankIndex);

	void SetCanWritePrgMemory(bool enabled) { m_canWritePrgMemory = enabled; }
	void SetCanWriteChrMemory(bool enabled) { m_canWriteChrMemory = enabled; }

private:
	size_t m_numPrgBanks;
	size_t m_numChrBanks;
	std::array<size_t, kPrgBankCount> m_prgBankIndices;
	std::array<size_t, kChrBankCount> m_chrBankIndices;
	bool m_canWritePrgMemory;
	bool m_canWriteChrMemory;
};


FORCEINLINE void Mapper::SetPrgBankIndex4k(size_t cpuBankIndex, size_t cartBankIndex)
{
	m_prgBankIndices[cpuBankIndex] = cartBankIndex;
}

FORCEINLINE void Mapper::SetPrgBankIndex8k(size_t cpuBankIndex, size_t cartBankIndex)
{
	cpuBankIndex *= 2;
	cartBankIndex *= 2;
	m_prgBankIndices[cpuBankIndex] = cartBankIndex++;
	m_prgBankIndices[cpuBankIndex + 1] = cartBankIndex + 1;
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

FORCEINLINE void Mapper::SetChrBankIndex1k(size_t ppuBankIndex, size_t cartBankIndex)
{
	m_chrBankIndices[ppuBankIndex] = cartBankIndex;
}

FORCEINLINE void Mapper::SetChrBankIndex8k(size_t ppuBankIndex, size_t cartBankIndex)
{
	ppuBankIndex *= 8;
	cartBankIndex *= 8;
	m_chrBankIndices[ppuBankIndex] = cartBankIndex++;
	m_chrBankIndices[ppuBankIndex + 1] = cartBankIndex + 1;
	m_chrBankIndices[ppuBankIndex + 2] = cartBankIndex + 2;
	m_chrBankIndices[ppuBankIndex + 3] = cartBankIndex + 3;
	m_chrBankIndices[ppuBankIndex + 4] = cartBankIndex + 4;
	m_chrBankIndices[ppuBankIndex + 5] = cartBankIndex + 5;
	m_chrBankIndices[ppuBankIndex + 6] = cartBankIndex + 6;
	m_chrBankIndices[ppuBankIndex + 7] = cartBankIndex + 7;
}
