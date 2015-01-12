#pragma once

#include "Base.h"
#include <cstring> 

enum class NameTableMirroring
{
	Horizontal,
	Vertical,
	FourScreen,
	OneScreenUpper,
	OneScreenLower,

	Undefined
};

#pragma pack(push)
#pragma pack(1)
struct RomHeader
{
	uint8 name[4];
	uint8 prgRomUnits; // in 16kb units
	uint8 chrRomUnits; // in 8 kb units (if 0, board uses CHR RAM)
	uint8 flags6;
	uint8 flags7;
	uint8 prgRamUnits; // in 8kb units *IGNORE*: Recently added to spec, most roms don't have this set
	uint8 flags9;
	uint8 flags10; // Unofficial
	uint8 zero[5]; // Should all be 0

	size_t GetPrgRomSizeBytes() const
	{
		return prgRomUnits * KB(16);
	}

	// If 0, board uses CHR RAM
	size_t GetChrRomSizeBytes() const
	{
		return chrRomUnits * KB(8);
	}

	NameTableMirroring GetNameTableMirroring() const
	{
		if (flags6 & 0x80)
			return NameTableMirroring::FourScreen;

		return (flags6 & 0x01)==1? NameTableMirroring::Vertical : NameTableMirroring::Horizontal;
	}

	uint8 GetMapperNumber() const
	{
		const uint8 result = (flags7 & 0xF0) | ((flags6 & 0xF0)>>4);
		return result;
	}

	// SRAM in CPU $6000-$7FFF, if present, is battery backed
	bool HasSRAM() const
	{
		return (flags6 & 0x02) != 0;
	}

	bool HasTrainer() const
	{
		return (flags6 & 0x04) != 0;
	}

	bool IsVSUnisystem() const
	{
		return (flags7 & 0x01) != 0;
	}

	// 8KB of Hint Screen data stored after CHR data
	bool IsPlayChoice10() const
	{
		return (flags7 & 0x02) != 0;
	}

	bool IsNES2Header() const
	{
		return (flags7 & 0xC0) == 2;
	}

	bool IsValidHeader() const
	{
		if (memcmp((const char*)name, "NES\x1A", 4) != 0)
			return false;

		// A general rule of thumb: if the last 4 bytes are not all zero, and the header is not marked for
		// NES 2.0 format, an emulator should either mask off the upper 4 bits of the mapper number or simply
		// refuse to load the ROM.
		if ( (zero[1]!=0 || zero[2]!=0 || zero[3]!=0 || zero[4]!=0) && IsNES2Header() )
			return false;

		return true;
	}
};
#pragma pack(pop)
static_assert(sizeof(RomHeader)==16, "RomHeader must be 16 bytes");
