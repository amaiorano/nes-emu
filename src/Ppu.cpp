#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"
#include "Debugger.h"

namespace
{
	const size_t kNumPaletteColors = 56;

	Color4 g_paletteColors[kNumPaletteColors] = {0};

	void InitPaletteColors()
	{
		// 2C03 and 2C05 palettes (http://wiki.nesdev.com/w/index.php/PPU_palettes#2C03_and_2C05)
		struct DAC3 { uint8 r, g, b; };
		DAC3 dac3Palette[] =
		{
			{3,3,3},{0,1,4},{0,0,6},{3,2,6},{4,0,3},{5,0,3},{5,1,0},{4,2,0},{3,2,0},{1,2,0},{0,3,1},{0,4,0},{0,2,2},{0,0,0},{0,0,0},{0,0,0},
			{5,5,5},{0,3,6},{0,2,7},{4,0,7},{5,0,7},{7,0,4},{7,0,0},{6,3,0},{4,3,0},{1,4,0},{0,4,0},{0,5,3},{0,4,4},{0,0,0},{0,0,0},{0,0,0},
			{7,7,7},{3,5,7},{4,4,7},{6,3,7},{7,0,7},{7,3,7},{7,4,0},{7,5,0},{6,6,0},{3,6,0},{0,7,0},{2,7,6},{0,7,7},{0,0,0},{0,0,0},{0,0,0},
			{7,7,7},{5,6,7},{6,5,7},{7,5,7},{7,4,7},{7,5,5},{7,6,4},{7,7,2},{7,7,3},{5,7,2},{4,7,3},{2,7,6},{4,6,7},{0,0,0},{0,0,0},{0,0,0}
		};
		//static_assert(ARRAYSIZE(dac3Palette)==kNumPaletteColors, "Invalid palette size");

		for (uint8 i = 0; i < kNumPaletteColors; ++i)
		{
			const DAC3& c = dac3Palette[i];
			g_paletteColors[i].SetRGBA((uint8(c.r/7.f*255.f)), ((uint8)(c.g/7.f*255.f)), ((uint8)(c.b/7.f*255.f)), 0xFF);
		}
	}
}

namespace PpuControl1 // $2000 (W)
{
	enum Type : uint8
	{
		NameTableAddressMask			= BIT(0)|BIT(1),
		PpuAddressIncrement				= BIT(2), // 0 = 1 byte, 1 = 32 bytes
		SpritePatternTableAddress		= BIT(3), // 0 = $0000, 1 = $1000 in VRAM
		BackgroundPatternTableAddress	= BIT(4), // 0 = $0000, 1 = $1000 in VRAM
		SpriteSize						= BIT(5), // 0 = 8x8, 1 = 8x16
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
		DisplayType				= BIT(0), // 0 = Color, 1 = Monochrome
		BackgroundClipping		= BIT(1), // 0 = BG invisible in left 8-pixel column, 1 = No clipping
		SpriteClipping			= BIT(2), // 0 = Sprites invisible in left 8-pixel column, 1 = No clipping
		BackgroundVisible		= BIT(3), // 0 = Background not displayed, 1 = Background visible
		SpriteVisible			= BIT(4), // 0 = Sprites not displayed, 1 = Sprites visible		
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

Ppu::Ppu()
	: m_ppuMemoryBus(nullptr)
	, m_nes(nullptr)
	, m_renderer(new Renderer())
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
	m_vramAddressHigh = true;

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

			if (m_ppuControlReg2->Test(PpuControl2::BackgroundVisible))
			{
				Render();
			}
			else
			{
				m_renderer->Clear();
				m_renderer->Render();
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
			m_vramAddressHigh = true;
		}
		break;

	case CpuMemory::kPpuVRamIoReg: // $2007
		{
			assert(m_vramAddressHigh && "User code error: trying to read from $2007 when VRAM address not yet fully set via $2006");

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

	case CpuMemory::kPpuVRamAddressReg2: // $2006
		{
			const uint16 halfAddress = TO16(value);
			if (m_vramAddressHigh)
				m_vramAddress = (halfAddress << 8) | (m_vramAddress & 0x00FF);
			else
				m_vramAddress = halfAddress | (m_vramAddress & 0xFF00);

			m_vramAddressHigh = !m_vramAddressHigh; // Toggle high/low
		}
		break;

	case CpuMemory::kPpuVRamIoReg: // $2007
		{
			assert(m_vramAddressHigh && "User code error: trying to write to $2007 when VRAM address not yet fully set via $2006");

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
	
	//@TODO: Map according to nametable mirroring (4K -> 2K). For now, we assume vertical mirroring (horizontal scrolling).
	const uint16 physicalVRamAddress = virtualVRamAddress % NameTableMemory::Size;
	
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

			tile[0][y] = (paletteUpperBits << 2) | (byte1.TestPos(7) | (byte2.TestPos(7) << 1));
			tile[1][y] = (paletteUpperBits << 2) | (byte1.TestPos(6) | (byte2.TestPos(6) << 1));
			tile[2][y] = (paletteUpperBits << 2) | (byte1.TestPos(5) | (byte2.TestPos(5) << 1));
			tile[3][y] = (paletteUpperBits << 2) | (byte1.TestPos(4) | (byte2.TestPos(4) << 1));
			tile[4][y] = (paletteUpperBits << 2) | (byte1.TestPos(3) | (byte2.TestPos(3) << 1));
			tile[5][y] = (paletteUpperBits << 2) | (byte1.TestPos(2) | (byte2.TestPos(2) << 1));
			tile[6][y] = (paletteUpperBits << 2) | (byte1.TestPos(1) | (byte2.TestPos(1) << 1));
			tile[7][y] = (paletteUpperBits << 2) | (byte1.TestPos(0) | (byte2.TestPos(0) << 1));

			++byte1Address;
			++byte2Address;
		}
	};

	static auto DrawTile = [&] (Renderer& renderer, int x, int y, uint8 tile[8][8])
	{
		Color4 color;

		for (uint16 ty = 0; ty < 8; ++ty)
		{
			for (size_t tx = 0; tx < 8; ++tx)
			{
				const uint8 paletteOffset = tile[tx][ty];

				//@TODO: Need a separate MapPpuToPalette that always maps every 4th byte to 0 (bg color)
				const uint8 paletteIndex = m_palette.Read( MapPpuToPalette(PpuMemory::kPalettesBase + paletteOffset) );
				assert(paletteIndex < kNumPaletteColors);
				const Color4& color = g_paletteColors[paletteIndex];

				renderer.DrawPixel(x + tx, y + ty, color);
			}
		}
	};
	
	const uint16 currBgPatternTableAddress = PpuControl1::GetBackgroundPatternTableAddress(m_ppuControlReg1->Value());
	const uint16 currNameTableAddress = PpuControl1::GetNameTableAddress(m_ppuControlReg1->Value());
	const uint16 currAttributeTableAddress = PpuControl1::GetAttributeTableAddress(m_ppuControlReg1->Value());

	uint8 tile[8][8] = {0};
	for (uint8 y = 0; y < 30; ++y)
	{
		for (uint8 x = 0; x < 32; ++x)
		{
			// Name table is a table of 32x30 1 byte tile indices
			const uint16 tileIndexAddress = currNameTableAddress + y * 32 + x;
			const uint8 tileIndex = m_ppuMemoryBus->Read(tileIndexAddress);

			const uint8 paletteUpperBits = GetTilePaletteUpperBits(currAttributeTableAddress, x, y);

			// Read in the tile data for tileIndex into the 8x8 tile from the pattern table (actual image data on the cart)
			ReadTile(currBgPatternTableAddress, tileIndex, paletteUpperBits, tile);
			
			DrawTile(*m_renderer, x*8, y*8, tile);
		}
	}

	m_renderer->Render();
}
