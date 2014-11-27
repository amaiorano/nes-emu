#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"
#include "MemoryMap.h"
#include "Debugger.h"
#include <tuple>

namespace
{
	const size_t kNumPaletteColors = 64; // Technically 56 but there is space for 64 and some games access >= 56
	Color4 g_paletteColors[kNumPaletteColors] = {0};

	void InitPaletteColors()
	{
		struct RGB { uint8 r, g, b; };

	#define USE_PALETTE 2

	#if USE_PALETTE == 1
		// This palette seems more "accurate"
		// 2C03 and 2C05 palettes (http://wiki.nesdev.com/w/index.php/PPU_palettes#2C03_and_2C05)
		RGB dac3Palette[] =
		{
			{3,3,3},{0,1,4},{0,0,6},{3,2,6},{4,0,3},{5,0,3},{5,1,0},{4,2,0},{3,2,0},{1,2,0},{0,3,1},{0,4,0},{0,2,2},{0,0,0},{0,0,0},{0,0,0},
			{5,5,5},{0,3,6},{0,2,7},{4,0,7},{5,0,7},{7,0,4},{7,0,0},{6,3,0},{4,3,0},{1,4,0},{0,4,0},{0,5,3},{0,4,4},{0,0,0},{0,0,0},{0,0,0},
			{7,7,7},{3,5,7},{4,4,7},{6,3,7},{7,0,7},{7,3,7},{7,4,0},{7,5,0},{6,6,0},{3,6,0},{0,7,0},{2,7,6},{0,7,7},{0,0,0},{0,0,0},{0,0,0},
			{7,7,7},{5,6,7},{6,5,7},{7,5,7},{7,4,7},{7,5,5},{7,6,4},{7,7,2},{7,7,3},{5,7,2},{4,7,3},{2,7,6},{4,6,7},{0,0,0},{0,0,0},{0,0,0}
		};

		for (uint8 i = 0; i < kNumPaletteColors; ++i)
		{
			const RGB& c = dac3Palette[i];
			g_paletteColors[i].SetRGBA((uint8(c.r/7.f*255.f)), ((uint8)(c.g/7.f*255.f)), ((uint8)(c.b/7.f*255.f)), 0xFF);
		}
	#elif USE_PALETTE == 2

		// This palette seems closer to what fceux does by default
		// http://nesdev.com/NESTechFAQ.htm#accuratepal

		RGB palette[] =
		{
			{0x80,0x80,0x80}, {0x00,0x3D,0xA6}, {0x00,0x12,0xB0}, {0x44,0x00,0x96},
			{0xA1,0x00,0x5E}, {0xC7,0x00,0x28}, {0xBA,0x06,0x00}, {0x8C,0x17,0x00},
			{0x5C,0x2F,0x00}, {0x10,0x45,0x00}, {0x05,0x4A,0x00}, {0x00,0x47,0x2E},
			{0x00,0x41,0x66}, {0x00,0x00,0x00}, {0x05,0x05,0x05}, {0x05,0x05,0x05},
			{0xC7,0xC7,0xC7}, {0x00,0x77,0xFF}, {0x21,0x55,0xFF}, {0x82,0x37,0xFA},
			{0xEB,0x2F,0xB5}, {0xFF,0x29,0x50}, {0xFF,0x22,0x00}, {0xD6,0x32,0x00},
			{0xC4,0x62,0x00}, {0x35,0x80,0x00}, {0x05,0x8F,0x00}, {0x00,0x8A,0x55},
			{0x00,0x99,0xCC}, {0x21,0x21,0x21}, {0x09,0x09,0x09}, {0x09,0x09,0x09},
			{0xFF,0xFF,0xFF}, {0x0F,0xD7,0xFF}, {0x69,0xA2,0xFF}, {0xD4,0x80,0xFF},
			{0xFF,0x45,0xF3}, {0xFF,0x61,0x8B}, {0xFF,0x88,0x33}, {0xFF,0x9C,0x12},
			{0xFA,0xBC,0x20}, {0x9F,0xE3,0x0E}, {0x2B,0xF0,0x35}, {0x0C,0xF0,0xA4},
			{0x05,0xFB,0xFF}, {0x5E,0x5E,0x5E}, {0x0D,0x0D,0x0D}, {0x0D,0x0D,0x0D},
			{0xFF,0xFF,0xFF}, {0xA6,0xFC,0xFF}, {0xB3,0xEC,0xFF}, {0xDA,0xAB,0xEB},
			{0xFF,0xA8,0xF9}, {0xFF,0xAB,0xB3}, {0xFF,0xD2,0xB0}, {0xFF,0xEF,0xA6},
			{0xFF,0xF7,0x9C}, {0xD7,0xE8,0x95}, {0xA6,0xED,0xAF}, {0xA2,0xF2,0xDA},
			{0x99,0xFF,0xFC}, {0xDD,0xDD,0xDD}, {0x11,0x11,0x11}, {0x11,0x11,0x11},
		};

		for (uint8 i = 0; i < kNumPaletteColors; ++i)
		{
			const RGB& c = palette[i];
			g_paletteColors[i].SetRGBA(c.r, c.g, c.b, 0xFF);
		}
	#endif
	}
}

namespace PpuControl1 // $2000 (W)
{
	enum Type : uint8
	{
		NameTableAddressMask			= BIT(0)|BIT(1),
		PpuAddressIncrement				= BIT(2), // 0 = 1 byte, 1 = 32 bytes
		SpritePatternTableAddress8x8	= BIT(3), // 0 = $0000, 1 = $1000 in VRAM only for 8x8 sprite size mode
		BackgroundPatternTableAddress	= BIT(4), // 0 = $0000, 1 = $1000 in VRAM
		SpriteSize8x16					= BIT(5), // 0 = 8x8, 1 = 8x16 (sprite size mode)
		PpuMasterSlaveSelect			= BIT(6), // 0 = Master, 1 = Slave (Unused)
		NmiOnVBlank						= BIT(7), // 0 = Disabled, 1 = Enabled
	};

	// Returns current name table address in VRAM ($2000, $2400, $2800, or $2C00)
	inline uint16 GetNameTableAddress(uint16 ppuControl1)
	{
		return PpuMemory::kNameTable0 + ((uint16)(ppuControl1 & NameTableAddressMask)) * PpuMemory::kNameAttributeTableSize;
	}

	inline uint16 GetAttributeTableAddress(uint16 ppuControl1)
	{
		return GetNameTableAddress(ppuControl1) + PpuMemory::kNameTableSize; // Follows name table
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
		DisplayType				= BIT(0), // 0 = Color, 1 = Monochrome
		BackgroundShowLeft8		= BIT(1), // 0 = BG invisible in left 8-pixel column, 1 = No clipping
		SpritesShowLeft8		= BIT(2), // 0 = Sprites invisible in left 8-pixel column, 1 = No clipping
		RenderBackground		= BIT(3), // 0 = Background not displayed, 1 = Background visible
		RenderSprites			= BIT(4), // 0 = Sprites not displayed, 1 = Sprites visible		
		ColorIntensityMask		= BIT(5)|BIT(6)|BIT(7), // High 3 bits if DisplayType == 0
		FullBackgroundColorMask	= BIT(5)|BIT(6)|BIT(7), // High 3 bits if DisplayType == 1
	};
}

namespace PpuStatus // $2002 (R)
{
	enum Type : uint8
	{
		VRAMWritesIgnored	= BIT(4),
		ScanlineSpritesGt8	= BIT(5),
		PpuHitSprite0		= BIT(6),
		InVBlank			= BIT(7),
	};
}

namespace
{
	std::tuple<uint16,uint8> GetSpritePatternTableAddressAndTileIndex(Bitfield8* ppuControlReg1, uint8 oamByte1)
	{
		uint16 address;
		uint8 tileIndex;
		if ( !ppuControlReg1->Test(PpuControl1::SpriteSize8x16) ) // 8x8 sprite
		{
			address = ppuControlReg1->Test(PpuControl1::SpritePatternTableAddress8x8)? 0x1000 : 0x0000;
			tileIndex = oamByte1;
		}
		else // 8x16 sprite, information is stored in oam byte 1
		{
			address = TestBits(oamByte1, BIT(0))? 0x1000 : 0x0000;
			tileIndex = ReadBits(oamByte1, ~BIT(0));
		}
		return std::make_tuple(address, tileIndex);
	}
}

Ppu::Ppu()
	: m_ppuMemoryBus(nullptr)
	, m_nes(nullptr)
	, m_renderer(new Renderer())
	, m_nameTableVerticalMirroring(false)
{
	InitPaletteColors();
	m_renderer->Create();
}

void Ppu::Initialize(PpuMemoryBus& ppuMemoryBus, Nes& nes)
{
	m_ppuMemoryBus = &ppuMemoryBus;
	m_nes = &nes;

	m_nameTables.Initialize();
	m_palette.Initialize();
	m_ppuRegisters.Initialize();

	m_ppuControlReg1 = m_ppuRegisters.RawPtrAs<Bitfield8*>(MapCpuToPpuRegister(CpuMemory::kPpuControlReg1));
	m_ppuControlReg2 = m_ppuRegisters.RawPtrAs<Bitfield8*>(MapCpuToPpuRegister(CpuMemory::kPpuControlReg2));
	m_ppuStatusReg = m_ppuRegisters.RawPtrAs<Bitfield8*>(MapCpuToPpuRegister(CpuMemory::kPpuStatusReg));
}

void Ppu::Reset()
{
	// See http://wiki.nesdev.com/w/index.php/PPU_power_up_state

	WritePpuRegister(CpuMemory::kPpuControlReg1, 0);
	WritePpuRegister(CpuMemory::kPpuControlReg2, 0);
	WritePpuRegister(CpuMemory::kPpuVRamAddressReg1, 0);
	WritePpuRegister(CpuMemory::kPpuVRamIoReg, 0);
	m_vramAndScrollFirstWrite = true;

	// Not necessary but helps with debugging
	m_vramAddress = 0xDDDD;
	m_vramBufferedValue = 0xDD;
}

void Ppu::Execute(bool& finishedRender, uint32& numCpuCyclesToExecute)
{
	finishedRender = false;
	
	const uint32 kNumVBlankCycles = 2273; // 20 scanlines
	const uint32 kNumRenderCycles = 27507; // 242 scanlines (1 prerender + 240 render + 1 postrender)

	enum State { Rendering, VBlanking };
	static State currState;
	static State nextState = VBlanking;

	currState = nextState;

	switch (currState)
	{	
	case Rendering:
		{
			m_ppuStatusReg->Clear(PpuStatus::InVBlank); // cleared at dot 1 of line 261 (start of pre-render line)

			if (m_ppuControlReg2->Test(PpuControl2::RenderBackground|PpuControl2::RenderSprites))
			{
				Render();
			}
			else
			{
				// We're in "forced vblank"
				ClearBackground();
				m_renderer->Present();
			}

			finishedRender = true;
			numCpuCyclesToExecute = kNumRenderCycles;

			nextState = VBlanking;
		}
		break;

	case VBlanking:
		{
			m_ppuStatusReg->Set(PpuStatus::InVBlank); // set at dot 1 of line 241 (line *after* post-render line)

			if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
				m_nes->SignalCpuNmi();

			numCpuCyclesToExecute = kNumVBlankCycles;

			nextState = Rendering;
		}
		break;
	}
}

uint8 Ppu::HandleCpuRead(uint16 cpuAddress)
{
	// CPU only has access to PPU memory-mapped registers
	assert(cpuAddress >= CpuMemory::kPpuRegistersBase && cpuAddress < CpuMemory::kPpuRegistersEnd);

	// If debugger is reading, we don't want any register side-effects, so just return the value
	if ( Debugger::IsExecuting() )
	{
		return ReadPpuRegister(cpuAddress);
	}

	uint8 result = 0;

	switch (cpuAddress)
	{
	case CpuMemory::kPpuStatusReg: // $2002
		{
			result = ReadPpuRegister(cpuAddress);

			m_ppuStatusReg->Clear(PpuStatus::InVBlank);
			WritePpuRegister(CpuMemory::kPpuVRamAddressReg1, 0);
			WritePpuRegister(CpuMemory::kPpuVRamAddressReg2, 0);
			m_vramAddress = 0;
			m_vramAndScrollFirstWrite = true;
		}
		break;

	case CpuMemory::kPpuVRamIoReg: // $2007
		{
			assert(m_vramAndScrollFirstWrite && "User code error: trying to read from $2007 when VRAM address not yet fully set via $2006");

			// Read from palette or return buffered value
			if (m_vramAddress >= PpuMemory::kPalettesBase)			
			{
				result = m_palette.Read(MapPpuToPalette(m_vramAddress));
			}
			else
			{
				result = m_vramBufferedValue;
			}

			// Write to register memory for debugging (not actually required)
			WritePpuRegister(cpuAddress, result);

			// Always update buffered value from current vram pointer before incrementing it.
			// Note that we don't buffer palette values, we read "under it", which mirrors the name table memory (VRAM/CIRAM).
			m_vramBufferedValue = m_ppuMemoryBus->Read(m_vramAddress);
				
			// Advance vram pointer
			const uint16 advanceOffset = PpuControl1::GetPpuAddressIncrementSize( m_ppuControlReg1->Value() );				
			m_vramAddress += advanceOffset;
		}
		break;

	default:
		result = ReadPpuRegister(cpuAddress);
	}

	return result;
}

void Ppu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	// Read old value
	const uint16 registerAddress = MapCpuToPpuRegister(cpuAddress);
	const uint8 oldValue = m_ppuRegisters.Read(registerAddress);

	// Update register value
	m_ppuRegisters.Write(registerAddress, value);

	switch (cpuAddress)
	{
	case CpuMemory::kPpuControlReg1: // $2000
		{
			const Bitfield8* oldPpuControlReg1 = reinterpret_cast<const Bitfield8*>(&oldValue);

			// The PPU pulls /NMI low if and only if both NMI_occurred and NMI_output are true. By toggling NMI_output ($2000 bit 7)
			// during vertical blank without reading $2002, a program can cause /NMI to be pulled low multiple times, causing multiple
			// NMIs to be generated. (http://wiki.nesdev.com/w/index.php/NMI)
			if ( !oldPpuControlReg1->Test(PpuControl1::NmiOnVBlank)
				&& m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank) // NmiOnVBlank toggled
				&& m_ppuStatusReg->Test(PpuStatus::InVBlank) ) // In vblank (and $2002 not read yet, which resets this bit)
			{
				m_nes->SignalCpuNmi();
			}
		}
		break;

	case CpuMemory::kPpuSprRamIoReg: // $2004
		{
			// Write value to sprite ram at address in $2003 (OAMADDR) and increment address
			const uint8 spriteRamAddress = ReadPpuRegister(CpuMemory::kPpuSprRamAddressReg);
			m_sprites.Write(spriteRamAddress, value);
			WritePpuRegister(CpuMemory::kPpuSprRamAddressReg, spriteRamAddress + 1);
		}
		break;

	case CpuMemory::kPpuVRamAddressReg1: // $2005 (PPUSCROLL)
		{
			if (m_vramAndScrollFirstWrite) // First write: X scroll values
			{
				m_scroll.fineOffsetX = ReadBits(value, 0x07);
				m_scroll.coarseOffsetX = ReadBits(value, ~0x07);
			}
			else // Second write: Y scroll values
			{
				m_scroll.fineOffsetY = ReadBits(value, 0x07);
				m_scroll.coarseOffsetY = ReadBits(value, ~0x07);
			}

			m_vramAndScrollFirstWrite = !m_vramAndScrollFirstWrite;
		}
		break;

	case CpuMemory::kPpuVRamAddressReg2: // $2006 (PPUADDR)
		{
			const uint16 halfAddress = TO16(value);
			if (m_vramAndScrollFirstWrite) // First write: high byte
			{
				m_vramAddress = (halfAddress << 8) | (m_vramAddress & 0x00FF);
			}
			else // Second write: low byte
			{
				m_vramAddress = halfAddress | (m_vramAddress & 0xFF00);
			}

			m_vramAndScrollFirstWrite = !m_vramAndScrollFirstWrite;
		}
		break;

	case CpuMemory::kPpuVRamIoReg: // $2007
		{
			assert(m_vramAndScrollFirstWrite && "User code error: trying to write to $2007 when VRAM address not yet fully set via $2006");

			// Write to palette or memory bus
			if (m_vramAddress >= PpuMemory::kPalettesBase)
			{
				m_palette.Write(MapPpuToPalette(m_vramAddress), value);
			}
			else
			{
				m_ppuMemoryBus->Write(m_vramAddress, value);
			}
				
			const uint16 advanceOffset = PpuControl1::GetPpuAddressIncrementSize( m_ppuControlReg1->Value() );
			m_vramAddress += advanceOffset;
		}
		break;
	}
}

uint8 Ppu::HandlePpuRead(uint16 ppuAddress)
{
	//@NOTE: The palette can only be accessed directly by the PPU (no address lines go out to Cartridge)
	return m_nameTables.Read(MapPpuToVRam(ppuAddress));
}

void Ppu::HandlePpuWrite(uint16 ppuAddress, uint8 value)
{
	m_nameTables.Write(MapPpuToVRam(ppuAddress), value);
}

uint16 Ppu::MapCpuToPpuRegister(uint16 cpuAddress)
{
	assert(cpuAddress >= CpuMemory::kPpuRegistersBase && cpuAddress < CpuMemory::kPpuRegistersEnd);
	return (cpuAddress - CpuMemory::kPpuRegistersBase ) % CpuMemory::kPpuRegistersSize;
}

uint16 Ppu::MapPpuToVRam(uint16 ppuAddress)
{
	assert(ppuAddress >= PpuMemory::kVRamBase); // Address may go into palettes (vram pointer)

	const uint16 virtualVRamAddress = (ppuAddress - PpuMemory::kVRamBase) % PpuMemory::kVRamSize;
	
	uint16 physicalVRamAddress = 0;
	if (m_nameTableVerticalMirroring)
	{
		// Vertical mirroring (horizontal scrolling)
		// A B
		// A B
		// Simplest case, just wrap >= 2K
		physicalVRamAddress = virtualVRamAddress % NameTableMemory::kSize;
	}
	else
	{
		// Horizontal mirroring (vertical scrolling)
		// A A
		// B B		
		if (virtualVRamAddress >= KB(3)) // 4th virtual page (B) maps to 2nd physical page
		{
			physicalVRamAddress = virtualVRamAddress - KB(2);
		}
		else if (virtualVRamAddress >= KB(2)) // 3rd virtual page (B) maps to 2nd physical page (B)
		{
			physicalVRamAddress = virtualVRamAddress - KB(1);
		}
		else if (virtualVRamAddress >= KB(1)) // 2nd virtual page (A) maps to 1st physical page (A)
		{
			physicalVRamAddress = virtualVRamAddress - KB(1);
		}
		else // 1st virtual page (A) maps to 1st physical page (A)
		{
			physicalVRamAddress = virtualVRamAddress;
		}
	}

	return physicalVRamAddress;
}

uint16 Ppu::MapPpuToPalette(uint16 ppuAddress)
{
	assert(ppuAddress >= PpuMemory::kPalettesBase && ppuAddress < PpuMemory::kPalettesEnd);

	uint16 paletteAddress = (ppuAddress - PpuMemory::kPalettesBase) % PpuMemory::kPalettesSize;

	// Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
	// If least 2 bits are unset, it's one of the 8 mirrored addresses, so clear bit 4 to mirror
	if ( !TestBits(paletteAddress, (BIT(1)|BIT(0))) )
	{
		ClearBits(paletteAddress, BIT(4));
	}

	return paletteAddress;
}

uint8 Ppu::ReadPpuRegister(uint16 cpuAddress)
{
	return m_ppuRegisters.Read(MapCpuToPpuRegister(cpuAddress));
}

void Ppu::WritePpuRegister(uint16 cpuAddress, uint8 value)
{
	m_ppuRegisters.Write(MapCpuToPpuRegister(cpuAddress), value);
}

void Ppu::ClearBackground()
{
	// Clear to palette color 0
	//@TODO: If vram address points to palette, we should render that palette color
	m_renderer->Clear(g_paletteColors[m_palette.Read(0)]);
}

void Ppu::Render()
{
	static auto GetTilePaletteUpperBits = [&] (uint16 attributeTableAddress, uint8 x, uint8 y)
	{
		const uint8 groupsPerScreenX = 8;
		const uint8 tilesPerGroupAxis = 4; // 4x4 = 16 tiles per group
		const uint8 tilesPerSquareAxis = 2; // 2x2 = 4 tiles per square (and 4x4 squares per group)
		const uint8 squaresPerGroupAxis = 2;

		// Get attribute byte for current group
		const uint8 groupX = x / tilesPerGroupAxis; assert(groupX < 8);
		const uint8 groupY = y / tilesPerGroupAxis; assert(groupY < 8);
		const uint8 groupIndex = groupY * groupsPerScreenX + groupX; assert(groupIndex < 64);
		const uint8 attributeByte = m_ppuMemoryBus->Read(attributeTableAddress + groupIndex);

		// Get square x,y relative to current group
		const uint8 squareX = (x - (groupX * tilesPerGroupAxis)) / squaresPerGroupAxis; assert(squareX < 2);
		const uint8 squareY = (y - (groupY * tilesPerGroupAxis))  / squaresPerGroupAxis; assert(squareY < 2);
		const uint8 squareIndex = squareY * tilesPerSquareAxis + squareX; assert(squareIndex < 4);

		// Compute tile's upper 2 palette bits (lower 2 bits are in tile data itself)
		const uint8 paletteUpperBits = (attributeByte >> (squareIndex*2)) & 0x3;
		assert(paletteUpperBits < 4);

		return paletteUpperBits;
	};

	static auto ReadTile = [&] (uint16 patternTableAddress, uint8 tileIndex, uint8 paletteUpperBits, uint8 tile[8][8])
	{
		// Every 16 bytes is 1 8x8 tile
		const uint16 tileOffset = TO16(tileIndex) * 16;
		assert(tileOffset+16 < KB(16) && "Indexing outside of current pattern table");

		uint16 byte1Address = patternTableAddress + tileOffset + 0;
		uint16 byte2Address = patternTableAddress + tileOffset + 8;

		for (uint16 y = 0; y < 8; ++y)
		{
			const uint8 b1 = m_ppuMemoryBus->Read(byte1Address);
			const uint8 b2 = m_ppuMemoryBus->Read(byte2Address);
			const Bitfield8& byte1 = reinterpret_cast<const Bitfield8&>(b1);
			const Bitfield8& byte2 = reinterpret_cast<const Bitfield8&>(b2);

			tile[0][y] = (paletteUpperBits << 2) | (byte1.TestPos01(7) | (byte2.TestPos01(7) << 1));
			tile[1][y] = (paletteUpperBits << 2) | (byte1.TestPos01(6) | (byte2.TestPos01(6) << 1));
			tile[2][y] = (paletteUpperBits << 2) | (byte1.TestPos01(5) | (byte2.TestPos01(5) << 1));
			tile[3][y] = (paletteUpperBits << 2) | (byte1.TestPos01(4) | (byte2.TestPos01(4) << 1));
			tile[4][y] = (paletteUpperBits << 2) | (byte1.TestPos01(3) | (byte2.TestPos01(3) << 1));
			tile[5][y] = (paletteUpperBits << 2) | (byte1.TestPos01(2) | (byte2.TestPos01(2) << 1));
			tile[6][y] = (paletteUpperBits << 2) | (byte1.TestPos01(1) | (byte2.TestPos01(1) << 1));
			tile[7][y] = (paletteUpperBits << 2) | (byte1.TestPos01(0) | (byte2.TestPos01(0) << 1));

			++byte1Address;
			++byte2Address;
		}
	};

	static auto DrawBackgroundTile = [&] (Renderer& renderer, int32 x, int32 y, uint8 tile[8][8])
	{
		for (uint16 ty = 0; ty < 8; ++ty)
		{
			for (uint16 tx = 0; tx < 8; ++tx)
			{
				const uint8 paletteOffset = tile[tx][ty];

				if (paletteOffset % 4 == 0) // Don't draw bg color pixel as screen was already cleared with that color
					continue;

				//@TODO: Need a separate MapPpuToPalette that always maps every 4th byte to 0 (bg color)
				const uint8 paletteIndex = m_palette.Read( MapPpuToPalette(PpuMemory::kImagePalette + paletteOffset) );
				assert(paletteIndex < kNumPaletteColors);

				const Color4& color = g_paletteColors[paletteIndex];

				const int32 xf = x + tx;
				const int32 yf = y + ty;

				// Clip x against right screen edge and y against bottom screen edge
				if (xf >= kScreenWidth || yf >= kScreenHeight)
					continue;

				renderer.DrawPixel(xf, yf, color);
			}
		}
	};

	static auto DrawSpriteTile = [&] (Renderer& renderer, int32 x, int32 y, uint8 tile[8][8], bool flipHorz, bool flipVert)
	{
		for (uint16 ty = 0; ty < 8; ++ty)
		{
			for (uint16 tx = 0; tx < 8; ++tx)
			{
				const uint8 paletteOffset = tile[tx][ty];

				if (paletteOffset % 4 == 0) // Don't draw transparent pixel
					continue;

				//@TODO: Need a separate MapPpuToPalette that always maps every 4th byte to 0 (bg color)
				const uint8 paletteIndex = m_palette.Read( MapPpuToPalette(PpuMemory::kSpritePalette + paletteOffset) );
				assert(paletteIndex < kNumPaletteColors);

				const Color4& color = g_paletteColors[paletteIndex];
				
				int32 xf = x + (flipHorz? 7 - tx : tx);
				int32 yf = y + (flipVert? 7 - ty : ty);

				// Clip x against right screen edge and y against bottom screen edge
				if (xf >= kScreenWidth || yf >= kScreenHeight)
					continue;

				renderer.DrawPixel(xf, yf, color);
			}
		}
	};

	static auto RenderSprites = [&] (bool behindBackground)
	{
		if (m_ppuControlReg2->Test(PpuControl2::RenderSprites))
		{
			const bool spriteSize8x8 = !m_ppuControlReg1->Test(PpuControl1::SpriteSize8x16);

			// Iterate backwards for sprite rendering priority (smaller indices draw over larger indices)
			for (size_t i = kMaxSprites; i-- > 0; )
			{
				const uint8* spriteData = m_sprites.RawPtr(static_cast<uint16>(i * kSpriteDataSize));

				// Note that sprites are drawn one scanline late, so we must add 1 to the y provided.
				// Client code must subtract 1 from the desired Y. This also implies that sprites can
				// never be drawn at the first visible scanline (Y = 0).
				const uint8 y = spriteData[0];
				if (y >= 239) // Don't render sprites clipped by bottom of screen
					continue;

				const uint8 x = spriteData[3];
				if (x < 8 && !m_ppuControlReg2->Test(PpuControl2::SpritesShowLeft8))
					continue;

				const uint8 attribs = spriteData[2];
				const bool currBehindBackground = TestBits(attribs, BIT(5));
				if (behindBackground != currBehindBackground)
					continue;

				uint16 patternTableAddress;
				uint8 tileIndex;
				std::tie(patternTableAddress, tileIndex) = GetSpritePatternTableAddressAndTileIndex(m_ppuControlReg1, spriteData[1]);

				uint8 tile[8][8] = {0};
				const uint8 paletteUpperBits = ReadBits(attribs, 0x3);
				const bool flipHorz = TestBits(attribs, BIT(6));
				const bool flipVert = TestBits(attribs, BIT(7));

				if (spriteSize8x8)
				{
					ReadTile(patternTableAddress, tileIndex, paletteUpperBits, tile);
					DrawSpriteTile(*m_renderer, x, y + 1, tile, flipHorz, flipVert);
				}
				else
				{
					const int32 y1 = flipVert? y + 1 + 8 : y + 1;
					const int32 y2 = flipVert? y + 1 : y + 1 + 8;

					ReadTile(patternTableAddress, tileIndex, paletteUpperBits, tile);
					DrawSpriteTile(*m_renderer, x, y1, tile, flipHorz, flipVert);

					ReadTile(patternTableAddress, tileIndex + 1, paletteUpperBits, tile);
					DrawSpriteTile(*m_renderer, x, y2, tile, flipHorz, flipVert);
				}
			}
		}
	};

	static auto RenderBackground = [&] ()
	{
		const uint8 kScreenTilesX = 32;
		const uint8 kScreenTilesY = 30;

		if (!m_ppuControlReg2->Test(PpuControl2::RenderBackground))
			return;

		const uint16 patternTableAddress = PpuControl1::GetBackgroundPatternTableAddress(m_ppuControlReg1->Value());
		const uint16 nameTableAddress = PpuControl1::GetNameTableAddress(m_ppuControlReg1->Value());
		const uint16 attributeTableAddress = PpuControl1::GetAttributeTableAddress(m_ppuControlReg1->Value());

		const uint8 initialX = m_ppuControlReg2->Test(PpuControl2::BackgroundShowLeft8)? 0 : 1;

		uint8 tile[8][8] = {0};

		uint8 scrollTileOffsetX = m_scroll.coarseOffsetX / 8;
		uint8 scrollTileOffsetY = m_scroll.coarseOffsetY / 8;

		// Render 32x30 tiles for the screen, plus 1 extra on each side for scrolling (we can see 33x31 tiles)
		for (uint8 screenTileY = 0; screenTileY < kScreenTilesY + 1; ++screenTileY)
		{
			for (uint8 screenTileX = initialX; screenTileX < kScreenTilesX + 1; ++screenTileX)
			{
				// Compute actual tile coordinate, taking scrolling offsets into account
				uint8 tileX = screenTileX + scrollTileOffsetX;
				uint8 tileY = screenTileY + scrollTileOffsetY;

				// The "virtual" tile space is 4x4 screens, so we need to adjust base nameTableAddress
				// and tile coordinates if the tile falls onto another screen
				uint16 addressOffset = 0;

				if (scrollTileOffsetY >= kScreenTilesY)
				{
					// Special case for when Y coarse scroll is set outside of valid range [0,29]
					// In that case, we don't switch name tables, but instead, address into attribute memory.
					// The effect is rendering one or two rows of attribute data as tiles at the top of the screen.
					// See: http://wiki.nesdev.com/w/index.php/The_skinny_on_NES_scrolling#Y_increment
					tileY %= 32; // Wrap around (5 bits hold this value in hardware)
				}
				else if (tileY >= kScreenTilesY) // If below current screen, switch vertical nametable
				{
					addressOffset += PpuMemory::kNameAttributeTableSize * 2;
					tileY %= kScreenTilesY; // Tile Y within next screen
				}
				
				if (tileX >= kScreenTilesX) // If to the right of current screen, switch horizontal nametable
				{
					addressOffset += PpuMemory::kNameAttributeTableSize;
					tileX %= kScreenTilesX; // Tile X within next screen
				}

				const uint16 tileIndexAddress = (nameTableAddress + addressOffset) + (tileY * kScreenTilesX) + tileX;
				const uint8 tileIndex = m_ppuMemoryBus->Read(tileIndexAddress);
				const uint8 paletteUpperBits = GetTilePaletteUpperBits(attributeTableAddress + addressOffset, tileX, tileY);

				ReadTile(patternTableAddress, tileIndex, paletteUpperBits, tile);

				const int32 xf = screenTileX * 8 - m_scroll.fineOffsetX;
				const int32 yf = screenTileY * 8 - m_scroll.fineOffsetY;
				DrawBackgroundTile(*m_renderer, xf, yf, tile);
			}
		}
	};

	// Always clear using palette color 0. When rendering bg tiles, we don't render color 0 so that
	// sprites below bg remain there.	
	ClearBackground();

	// Sprites behind background
	RenderSprites(true);

	// Background
	RenderBackground();

	// Sprites above background
	RenderSprites(false);

	m_renderer->Present();
}
