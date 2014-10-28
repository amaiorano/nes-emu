#pragma once
#include "Base.h"
#include <array>

namespace PpuControl1 // $2000 (W)
{
	enum Type : uint8
	{
		NameTableAddressMask			= 0x03,
		PpuAddressIncrement				= 0x04, // 0 = 1 byte, 1 = 32 bytes
		SpritePatternTableAddress		= 0x08, // 0 = $0000, 1 = $1000 in VRAM
		BackgroundPatternTableAddress	= 0x10, // 0 = $0000, 1 = $1000 in VRAM
		SpriteSize						= 0x20, // 0 = 8x8, 1 = 8x16
		PpuMasterSlaveSelect			= 0x40, // 0 = Master, 1 = Slave (Unused)
		NmiOnVBlank						= 0x80, // 0 = Disabled, 1 = Enabled
	};

	// Returns current name table address in VRAM ($2000, $2400, $2800, or $2C00)
	inline uint16 GetNameTableAddress(uint16 ppuControl1)
	{
		return 0x2000 + ((uint16)(ppuControl1 & NameTableAddressMask)) * 0x0400;
	}

	inline uint16 GetSpritePatternTableAddress(uint16 ppuControl1)
	{
		return (ppuControl1 & SpritePatternTableAddress)? 0x1000 : 0x0000;
	}

	inline uint16 GetBackgroundPatternTableAddress(uint16 ppuControl1)
	{
		return (ppuControl1 & BackgroundPatternTableAddress)? 0x1000 : 0x0000;
	}

	inline uint16 GetPpuAddressIncrementSize(uint16 ppuControl1)
	{
		return (ppuControl1 & PpuAddressIncrement)? 32 : 1;
	}
}

namespace PpuControl2 // $2001 (W)
{
	enum Type : uint8
	{
		DisplayType = 0x01, // 0 = Color, 1 = Monochrome
		BackgroundClipping = 0x02, // 0 = BG invisible in left 8-pixel column, 1 = No clipping
		SpriteClipping = 0x04, // 0 = Sprites invisible in left 8-pixel column, 1 = No clipping
		BackgroundVisible = 0x08, // 0 = Background not displayed, 1 = Background visible
		SpriteVisible = 0x10, // 0 = Sprites not displayed, 1 = Sprites visible		
		ColorIntensityMask = 0xE0, // High 3 bits if DisplayType == 0
		FullBackgroundColorMask = 0xE0, // High 3 bits if DisplayType == 1
	};
}

namespace PpuStatus // $2002 (R)
{
	enum Type : uint8
	{
		VRAMWritesIgnored	= 0x10,
		ScanlineSpritesGt8	= 0x20,
		PpuHitSprite0		= 0x40,
		InVBlank			= 0x80,
	};
}

template <typename Derived, size_t memorySize>
class MemoryBase
{
public:
	MemoryBase()
	{
		Reset();
	}

	void Reset()
	{
		m_memory.fill(0);
	}

	uint8 Read8(uint16 address) const
	{
		return m_memory[Derived::WrapMirroredAddress(address)];
	}

	uint16 Read16(uint16 address) const
	{
		address = Derived::WrapMirroredAddress(address);
		return TO16(m_memory[address]) | (TO16(m_memory[address + 1]) << 8);
	}

	void Write8(uint16 address, uint8 value)
	{
		address = Derived::WrapMirroredAddress(address);
		m_memory[address] = value;
	}

	void Write16(uint16 address, uint16 value)
	{
		address = Derived::WrapMirroredAddress(address);
		m_memory[address] = value & 0x00FF;
		m_memory[address + 1] = value >> 8;
	}

	// Use with care! No wrapping for mirrored addresses is performed
	uint8* UnsafePtr(uint16 address)
	{
		return &m_memory[address];
	}

	// Casts result to T*
	template <typename T>
	T* UnsafePtrAs(uint16 address)
	{
		return reinterpret_cast<T*>(UnsafePtr(address));
	}

protected:
	std::array<uint8, memorySize> m_memory;
};

// RAM
class CpuRam : public MemoryBase<CpuRam, KB(64)>
{
public:
	static const uint16 kPrgRomMaxSize	= KB(32);
	static const uint16 kPrgRomLowBase	= 0x8000;
	static const uint16 kPrgRomHighBase	= 0xC000;
	static const uint16 kStackBase		= 0x0100; // Range [$0100,$01FF]
	static const uint16 kNmiVector		= 0xFFFA; // and 0xFFFB
	static const uint16 kResetVector	= 0xFFFC; // and 0xFFFD
	static const uint16 kIrqVector		= 0xFFFE; // and 0xFFFF

	// PPU memory-mapped registers
	static const uint16 kPpuControl1		= 0x2000; // (W)
	static const uint16 kPpuControl2		= 0x2001; // (W)
	static const uint16 kPpuStatus			= 0x2002; // (R)
	static const uint16 kPpuSprRamAddress	= 0x2003; // (W) \_
	static const uint16 kPpuSprRamIO		= 0x2004; // (W) /
	static const uint16 kPpuVRamAddress1	= 0x2005; // (W2)
	static const uint16 kPpuVRamAddress2	= 0x2006; // (W2) \_
	static const uint16 kPpuVRamIO			= 0x2007; // (RW) /

	void LoadPrgRom(uint8* pPrgRom, size_t size)
	{
		switch (size)
		{
		case KB(16):
			memcpy(&m_memory[kPrgRomLowBase], pPrgRom, size);
			memcpy(&m_memory[kPrgRomHighBase], pPrgRom, size);
			break;
		
		case KB(32):
			memcpy(&m_memory[kPrgRomLowBase], pPrgRom, size);
			break;
		
		default:
			throw std::invalid_argument("Invalid PrgRom size");
		}
	}

	static uint16 WrapMirroredAddress(uint16 address)
	{
		if (address < 0x2000) // [$0800,$2000[ mirrors [$0000,$0800[
		{
			return address & ~(0x2000 - 0x800);
		}
		else if (address < 0x4000) // [$2008,$4000[ mirrors [$2000,$2008[
		{
			return address & ~(0x4000 - 0x2008);
		}
		return address;
	}
};

// VRAM
class PpuRam : public MemoryBase<PpuRam, KB(16)>
{
public:
	void LoadChrRom(uint8* pChrRom, size_t size)
	{
		assert(size == KB(8));
		memcpy(&m_memory[0], pChrRom, size);
	}

	static const uint16 kChrRomSize = KB(8); // Half the memory is CHR-ROM (2 pattern tables of 4kb each)
	static const uint16 kPatternTableSize = KB(4);
	static const uint16 kPatternTable0 = 0x0000;
	static const uint16 kPatternTable1 = 0x1000;

	// There are up to 4 Name/Attribute tables, each pair is 1 KB.
	// In fact, NES only has 2 KB total for name tables; the other 2 KB are mirrored off the first
	// two, either horizontally or vertically, or the cart supplies and extra 2 KB memory for 4 screen.
	static const uint16 kNameTableSize = 1024; // Size of name + attribute table
	static const uint16 kNameTable0 = 0x2000;
	static const uint16 kNameTable1 = kNameTable0 + kNameTableSize;
	static const uint16 kNameTablesEnd = kNameTable0 + kNameTableSize * 4;

	// This is not actually the palette, but the palette lookup table (indices into actual palette)
	static const uint16 kPaletteSize = 16;
	static const uint16 kImagePalette = 0x3F00;
	static const uint16 kSpritePalette = 0x3F10;
	static const uint16 kPalettesEnd = kImagePalette + kPaletteSize * 2;

	static uint16 WrapMirroredAddress(uint16 address)
	{
		// Name-table wrapping
		if (address >= kNameTable1 && address < kNameTablesEnd)
		{
			//@TODO: use screen arrangement to determine the type of wrapping (if any)
			assert(false && "TODO: Handle name-table wrapping");
		}

		// Palette: mirror every 4th byte of sprite palette to image palette
		else if (address >= kSpritePalette && address < kPalettesEnd 
			&& (address & 0x0003)==0) // If low 2 bits are 0 (multiple of 4)
		{
			return address & ~0x0010; // Turn off bit 5 (0x3F1x -> 0x3F0x)
		}
		return address;
	}	
};

// SPR-RAM or OAM (object attribute memory)
class SpriteRam : public MemoryBase<SpriteRam, 256>
{
public:
	FORCEINLINE static uint16 WrapMirroredAddress(uint16 /*address*/) {}
};
