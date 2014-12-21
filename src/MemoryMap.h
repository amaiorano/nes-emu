#pragma once

#include "Base.h"

// CPU address space locations constants/helpers
namespace CpuMemory
{
	const uint16 kInternalRamBase			= 0x0000;
	const uint16 kInternalRamSize			= KB(2);
	const uint16 kInternalRamEnd			= kInternalRamBase + kInternalRamSize * 4; // Mirrored

	const uint16 kPpuRegistersBase			= 0x2000;
	const uint16 kPpuRegistersSize			= 8;
	const uint16 kPpuRegistersEnd			= kPpuRegistersBase + kPpuRegistersSize * 1024; // Mirrored

	const uint16 kCpuRegistersBase			= 0x4000;
	const uint16 kCpuRegistersSize			= 32;
	const uint16 kCpuRegistersEnd			= kCpuRegistersBase + kCpuRegistersSize;

	const uint16 kExpansionRomBase			= 0x4020;
	const uint16 kExpansionRomSize			= KB(8) - kCpuRegistersSize;
	const uint16 kExpansionRomEnd			= kExpansionRomBase + kExpansionRomSize;

	const uint16 kSaveRamBase				= 0x6000;
	const uint16 kSaveRamSize				= KB(8);
	const uint16 kSaveRamEnd				= kSaveRamBase + kSaveRamSize;

	const uint16 kPrgRomBase				= 0x8000;
	const uint16 kPrgRomSize				= KB(32);
	const uint32 kProRomEnd					= kPrgRomBase + kPrgRomSize; // Note 32 bits
	
	// Validate that end matches next base
	static_assert(kInternalRamEnd == kPpuRegistersBase, "Invalid memory map");
	static_assert(kPpuRegistersEnd == kCpuRegistersBase, "Invalid memory map");
	static_assert(kCpuRegistersEnd == kExpansionRomBase, "Invalid memory map");
	static_assert(kExpansionRomEnd == kSaveRamBase, "Invalid memory map");
	static_assert(kSaveRamEnd == kPrgRomBase, "Invalid memory map");


	const uint16 kStackBase					= 0x0100; // Range [$0100,$01FF] (page 1)

	// PPU memory-mapped registers
	const uint16 kPpuControlReg1			= 0x2000; // (W)
	const uint16 kPpuControlReg2			= 0x2001; // (W)
	const uint16 kPpuStatusReg				= 0x2002; // (R)
	const uint16 kPpuSprRamAddressReg		= 0x2003; // (W) \_ OAMADDR
	const uint16 kPpuSprRamIoReg			= 0x2004; // (W) /  OAMDATA
	const uint16 kPpuVRamAddressReg1		= 0x2005; // (W2)
	const uint16 kPpuVRamAddressReg2		= 0x2006; // (W2) \_
	const uint16 kPpuVRamIoReg				= 0x2007; // (RW) /

	const uint16 kSpriteDmaReg				= 0x4014; // (W) OAMDMA
	const uint16 kControllerPort1			= 0x4016; // (RW) Strobe for both controllers (bit 0), and controller 1 output
	const uint16 kControllerPort2			= 0x4017; // (R) Controller 2 output

	const uint16 kNmiVector					= 0xFFFA; // and 0xFFFB
	const uint16 kResetVector				= 0xFFFC; // and 0xFFFD
	const uint16 kIrqVector					= 0xFFFE; // and 0xFFFF
}

// PPU address space locations constants/helpers
namespace PpuMemory
{
	// Addressible PPU is only 16K (14 bits)
	const uint16 kPpuMemorySize				= KB(16);

	// CHR-ROM stores pattern tables
	const uint16 kChrRomBase				= 0x0000;
	const uint16 kChrRomSize				= KB(8); // Half the memory is CHR-ROM (2 pattern tables of 4kb each)
	const uint16 kChrRomEnd					= kChrRomBase + kChrRomSize;

	// VRAM (aka CIRAM) stores name tables
	const uint16 kVRamBase					= 0x2000;
	const uint16 kVRamSize					= KB(4);
	const uint16 kVRamEnd					= kVRamBase + KB(8) - 256; // Mirrored

	const uint16 kPalettesBase				= 0x3F00;
	const uint16 kPalettesSize				= 32;
	const uint16 kPalettesEnd				= kPalettesBase + kPalettesSize * 8; // Mirrored	

	// Validate that end matches next base
	static_assert(kChrRomEnd == kVRamBase, "Invalid memory map");
	static_assert(kVRamEnd == kPalettesBase, "Invalid memory map");


	const uint16 kNumPatternTables			= 2;
	const uint16 kPatternTableSize			= KB(4);
	const uint16 kPatternTable0				= 0x0000;
	const uint16 kPatternTable1				= 0x1000;

	// There are up to 4 Name/Attribute tables, each pair is 1 KB.
	// In fact, NES only has 2 KB total for name tables; the other 2 KB are mirrored off the first
	// two, either horizontally or vertically, or the cart supplies and extra 2 KB memory for 4 screen.
	// Also, a "name table" includes the attribute table, which are in the last 64 bytes.
	const uint16 kNameTableSize				= 960;
	const uint16 kAttributeTableSize		= 64;
	const uint16 kNameAttributeTableSize	= kNameTableSize + kAttributeTableSize;

	const uint16 kNumMaxNameTables			= 4;
	const uint16 kNameTable0				= 0x2000;
	const uint16 kNameTable1				= kNameTable0 + kNameAttributeTableSize;
	const uint16 kNameTablesEnd				= kNameTable0 + kNameAttributeTableSize * 4;

	const uint16 kNumMaxAttributeTables		= 4;
	const uint16 kAttributeTable0			= kNameTable0 + kNameTableSize;

	// This is not actually the palette, but the palette lookup table (indices into actual palette)
	const uint16 kSinglePaletteSize			= kPalettesSize / 2;
	const uint16 kImagePalette				= 0x3F00;
	const uint16 kSpritePalette				= 0x3F10;

	inline uint16 GetPatternTableAddress(size_t index)
	{
		assert(index < kNumPatternTables);
		return static_cast<uint16>(kPatternTable0 + kPatternTableSize * index);
	}

	inline uint16 GetNameTableAddress(size_t index)
	{
		assert(index < kNumMaxNameTables);
		return static_cast<uint16>(kNameTable0 + kNameAttributeTableSize * index);
	}

	inline uint16 GetAttributeTableAddress(size_t index)
	{
		assert(index < kNumMaxAttributeTables);
		return static_cast<uint16>(kAttributeTable0 + kNameAttributeTableSize * index);
	}
}

