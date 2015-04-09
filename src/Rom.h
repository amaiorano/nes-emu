#pragma once

#include "Base.h"

enum class NameTableMirroring
{
	Horizontal,
	Vertical,
	FourScreen,
	OneScreenUpper,
	OneScreenLower,

	Undefined
};

enum class NesHeaderType
{
	iNesArchaic,
	iNes,
	Nes2
};

class RomHeader
{
public:
	// Initialize from first 16 bytes of rom file
	void Initialize(uint8 headerBytes[16]);

	NesHeaderType GetHeaderType() const { return m_nesHeaderType; }
	
	// Number of 16K CHR-ROM banks
	size_t GetNumPrgRomBanks() const { return m_prgRomBanks; }

	// Number of 8K CHR-ROM banks. If 0, board uses CHR-RAM.
	size_t GetNumChrRomBanks() const { return m_chrRomBanks; }
	
	// Total number of PRG-RAM (potentially) used. All/part/none of it
	// may be battery-backed (aka "SRAM").
	//@TODO: NES 2.0 splits into battery-backed and non.
	size_t GetNumPrgRamBanks() const { return m_prgRamBanks; }

	size_t GetPrgRomSizeBytes() const { return m_prgRomBanks * KB(16); }
	size_t GetChrRomSizeBytes() const { return m_chrRomBanks * KB(8); }
	size_t GetPrgRamSizeBytes() const { return m_prgRamBanks * KB(8); }
	
	NameTableMirroring GetNameTableMirroring() const { return m_mirroring; }
	
	uint8 GetMapperNumber() const { return m_mapperNumber; }
	
	bool HasSRAM() const { return m_hasSaveRam; }

	bool HasTrainer() const { return m_hasTrainer; }

	bool IsVSUnisystem() const { return m_isVSUnisystem; }

	// 8KB of Hint Screen data stored after CHR data
	bool IsPlayChoice10() const { return m_isPlayChoice10; }

private:
	NesHeaderType m_nesHeaderType;
	size_t m_prgRomBanks;
	size_t m_chrRomBanks;
	size_t m_prgRamBanks;
	NameTableMirroring m_mirroring;
	uint8 m_mapperNumber;
	bool m_hasSaveRam;
	bool m_hasTrainer;
	bool m_isVSUnisystem;
	bool m_isPlayChoice10;
};
