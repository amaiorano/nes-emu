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

	// EDC BA 98765 43210 *** NOTE bit 15 is missing because it's not used. PPU address space is 14 bits wide, but extra bit is used f or scrolling.
	// yyy NN YYYYY XXXXX
	// ||| || ||||| +++++-- coarse X scroll
	// ||| || +++++-------- coarse Y scroll
	// ||| ++-------------- nametable select
	// +++----------------- fine Y scroll
	FORCEINLINE void SetVRamAddressCoarseX(uint16& v, uint8 value)
	{
		v = (v & ~0x001F) | (TO16(value) & 0x001F);
	}

	FORCEINLINE uint8 GetVRamAddressCoarseX(const uint16& v)
	{
		return TO8(v & 0x001F);
	}

	FORCEINLINE void SetVRamAddressCoarseY(uint16& v, uint8 value)
	{
		v = (v & ~0x03E0) | (TO16(value) << 5);
	}

	FORCEINLINE uint8 GetVRamAddressCoarseY(const uint16& v)
	{
		return TO8((v >> 5) & 0x001F);
	}

	FORCEINLINE void SetVRamAddressNameTable(uint16& v, uint8 value)
	{
		v = (v & ~0x0C00) | (TO16(value) << 10);
	}

	FORCEINLINE void SetVRamAddressFineY(uint16& v, uint8 value)
	{
		v = (v & ~0x7000) | (TO16(value) << 12);
	}

	FORCEINLINE uint8 GetVRamAddressFineY(const uint16& v)
	{
		return TO8((v & 0x7000) >> 12);
	}

	FORCEINLINE void CopyVRamAddressHori(uint16& target, uint16& source)
	{
		// Copy coarse X (5 bits) and low nametable bit
		target = (target & ~0x041F) | ((source & 0x041F));
	}

	FORCEINLINE void CopyVRamAddressVert(uint16& target, uint16& source)
	{
		// Copy coarse Y (5 bits), fine Y (3 bits), and high nametable bit
		target = (target & 0x041F) | ((source & ~0x041F));
	}

	void IncHoriVRamAddress(uint16& v)
	{
		if ((v & 0x001F) == 31) // if coarse X == 31
		{
			v &= ~0x001F; // coarse X = 0
			v ^= 0x0400; // switch horizontal nametable
		}
		else
		{
			v += 1; // increment coarse X
		}
	}

	void IncVertVRamAddress(uint16& v)
	{
		if ((v & 0x7000) != 0x7000) // if fine Y < 7
		{
			v += 0x1000; // increment fine Y
		}
		else
		{
			v &= ~0x7000; // fine Y = 0
			uint16 y = (v & 0x03E0) >> 5; // let y = coarse Y
			if (y == 29)
			{
				y = 0; // coarse Y = 0
				v ^= 0x0800; // switch vertical nametable
			}
			else if (y == 31)
			{
				y = 0; // coarse Y = 0, nametable not switched
			}
			else
			{
				y += 1; // increment coarse Y
			}
			v = (v & ~0x03E0) | (y << 5); // put coarse Y back into v
		}
	};
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
		SpriteOverflow		= BIT(5),
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
	, m_rendererHolder(new Renderer())
	, m_renderer(m_rendererHolder.get())
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
	m_tempVRamAddress = 0xDDDD;
	m_vramBufferedValue = 0xDD;

	m_cycle = 0;
	m_evenFrame = true;
}

void Ppu::Execute(uint32 ppuCycles, bool& finishedRender)
{
	const size_t kNumTotalScanlines = 262;
	const size_t kNumHBlankAndBorderCycles = 85;
	const size_t kNumScanlineCycles = kScreenWidth + kNumHBlankAndBorderCycles; // 256 + 85 = 341
	const size_t kNumScreenCycles = kNumScanlineCycles * kNumTotalScanlines; // 89342 cycles per screen

	finishedRender = false;

	const bool renderingEnabled = m_ppuControlReg2->Test(PpuControl2::RenderBackground|PpuControl2::RenderSprites);

	for ( ; ppuCycles > 0; --ppuCycles)
	{
		const uint32 x = m_cycle % kNumScanlineCycles; // offset in current scanline
		const uint32 y = m_cycle / kNumScanlineCycles; // scanline

		if (x >= 258 && x <= 320) // HBlank?
		{
			// On pre-render line during sub-range of HBlank, we copy vertical values of t to v
			if (renderingEnabled && y == 261 && x >= 280 && x <= 304)
			{
				CopyVRamAddressVert(m_vramAddress, m_tempVRamAddress);
			}
		}
		else if ( (y >= 0 && y <= 239) || y == 261 ) // Visible and Pre-render scanlines
		{
			// Update VRAM address and fetch data
			if (renderingEnabled)
			{
				// PPU fetches 4 bytes every 8 cycles for a given tile (NT, AT, LowBG, and HighBG).
				// We want to know when we're on the last cycle of the HighBG tile byte (see Ntsc_timing.jpg)
				const bool lastFetchCycle = x >= 8 && (x % 8 == 0);

				if (lastFetchCycle)
				{
					FetchBackgroundTileData();

					// Data for v was just fetched, so we can now increment it
					if (x != 256)
					{
						IncHoriVRamAddress(m_vramAddress);
					}
					else
					{
						IncVertVRamAddress(m_vramAddress);
					}
				}
				else if (x == 257)
				{
					CopyVRamAddressHori(m_vramAddress, m_tempVRamAddress);
				}

				if (renderingEnabled && (x < kScreenWidth) && (y < kScreenHeight))
				{
					// Render pixel at x,y using pipelined fetch data
					RenderPixel(x, y);
				}
			}

			// Clear flags on pre-render line at dot 1
			if (y == 261 && x == 1)
			{
				m_ppuStatusReg->Clear(PpuStatus::InVBlank | PpuStatus::PpuHitSprite0 | PpuStatus::SpriteOverflow);
			}
			
			// Present on (second to) last cycle of last visible scanline
			//@TODO: Do this on last frame of post-render line?
			if (y == 239 && x == 339)
			{
				m_renderer->Present();
				ClearBackground();
				finishedRender = true;

				// For odd frames, the cycle at the end of the scanline (340,239) is skipped
				if (!m_evenFrame && renderingEnabled)
					++m_cycle;

				m_evenFrame = !m_evenFrame;
			}
		}
		else // Post-render and VBlank 240-260
		{
			assert(y >= 240 && y <= 260);

			if (y == 241 && x == 1)
			{
				m_ppuStatusReg->Set(PpuStatus::InVBlank);

				//@TODO: is this the right time?
				if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
					m_nes->SignalCpuNmi();
			}
		}

		// Update cycle
		m_cycle = (m_cycle + 1) % kNumScreenCycles;
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
			SetVRamAddressNameTable(m_tempVRamAddress, value & 0x3);

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
				m_fineX = value & 0x07;
				SetVRamAddressCoarseX(m_tempVRamAddress, (value & ~0x07) >> 3);
			}
			else // Second write: Y scroll values
			{
				SetVRamAddressFineY(m_tempVRamAddress, value & 0x07);
				SetVRamAddressCoarseY(m_tempVRamAddress, (value & ~0x07) >> 3);
			}

			m_vramAndScrollFirstWrite = !m_vramAndScrollFirstWrite;
		}
		break;

	case CpuMemory::kPpuVRamAddressReg2: // $2006 (PPUADDR)
		{
			const uint16 halfAddress = TO16(value);
			if (m_vramAndScrollFirstWrite) // First write: high byte
			{
				// Write 6 bits to high byte - note that technically we shouldn't touch bit 15, but whatever
				m_tempVRamAddress = ((halfAddress & 0x3F) << 8) | (m_tempVRamAddress & 0x00FF);
			}
			else
			{
				m_tempVRamAddress = (m_tempVRamAddress & 0xFF00) | halfAddress;
				m_vramAddress = m_tempVRamAddress; // Update v from t on second write
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

void Ppu::FetchBackgroundTileData()
{
	// Load bg tile row data (2 bytes) at v into pipeline
	const auto& v = m_vramAddress;
	const uint16 patternTableAddress = PpuControl1::GetBackgroundPatternTableAddress(m_ppuControlReg1->Value());
	const uint16 tileIndexAddress = 0x2000 | (v & 0x0FFF);
	const uint16 attributeAddress = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
	assert(attributeAddress >= PpuMemory::kAttributeTable0 && attributeAddress < PpuMemory::kNameTablesEnd);
	const uint8 tileIndex = m_ppuMemoryBus->Read(tileIndexAddress);
	const uint16 tileOffset = TO16(tileIndex) * 16;
	const uint8 fineY = GetVRamAddressFineY(v);
	const uint16 byte1Address = patternTableAddress + tileOffset + fineY;
	const uint16 byte2Address = byte1Address + 8;

	// Load attribute byte then compute and store the high palette bits from it for this tile
	// The high palette bits are 2 consecutive bits in the attribute byte. We need to shift it right
	// by 0, 2, 4, or 6 and read the 2 low bits. The amount to shift by is can be computed from the
	// VRAM address as follows: [bit 6, bit 2, 0]
	const uint8 attribute = m_ppuMemoryBus->Read(attributeAddress);
	const uint8 attributeShift = ((v & 0x40) >> 4) | (v & 0x2);
	assert(attributeShift == 0 || attributeShift == 2 || attributeShift == 4 || attributeShift == 6);
	const uint8 paletteHighBits = (attribute >> attributeShift) & 0x3;

	auto& currTile = m_bgTileFetchDataPipeline[0];
	auto& nextTile = m_bgTileFetchDataPipeline[1];

	currTile = nextTile; // Shift pipelined data

	// Push results at top of pipeline
	nextTile.lowBg = m_ppuMemoryBus->Read(byte1Address);
	nextTile.highBg = m_ppuMemoryBus->Read(byte2Address);
	nextTile.paletteHighBits = paletteHighBits;

#if CONFIG_DEBUG
	nextTile.debug.vramAddress = m_vramAddress;
	nextTile.debug.tileIndexAddress = tileIndexAddress;
	nextTile.debug.attributeAddress = attributeAddress;
	nextTile.debug.attributeShift = attributeShift;
	nextTile.debug.byte1Address = byte1Address;
#endif
}

void Ppu::RenderPixel(uint32 x, uint32 y)
{
	// At this point, the data for the current and next tile are in m_bgTileFetchDataPipeline
	const auto& currTile = m_bgTileFetchDataPipeline[0];
	const auto& nextTile = m_bgTileFetchDataPipeline[1];

	// Mux uses fine X to select a bit from shift registers
	const uint16 muxMask = 1 << (7 - m_fineX);

	// Instead of actually shifting every cycle, we rebuild the shift register values
	// for the current cycle (using the x value)
	//@TODO: We could optimize this storing 16 bit values for low and high BG bytes and
	// either doing the shift every cycle, or building 16 bit mask.
	const uint8 xShift = x % 8;
	const uint8 lowBg = (currTile.lowBg << xShift) | (nextTile.lowBg >> (8 - xShift));
	const uint8 highBg = (currTile.highBg << xShift) | (nextTile.highBg >> (8 - xShift));

	const uint8 paletteLowBits = (TestBits01(highBg, muxMask) << 1) | (TestBits01(lowBg, muxMask));

	// Technically, the mux would index 2 8-bit registers containing replicated values for the current
	// and next tile palette high bits (from attribute bytes), but this is faster.
	const uint8 paletteHighBits = (xShift + m_fineX < 8)? currTile.paletteHighBits : nextTile.paletteHighBits;

	// Compute offset into palette memory that contains the palette index
	const uint8 paletteOffset = (paletteHighBits << 2) | (paletteLowBits & 0x3);

	//@TODO: Need a separate MapPpuToPalette that always maps every 4th byte to 0 (bg color)
	const uint8 paletteIndex = m_palette.Read( MapPpuToPalette(PpuMemory::kImagePalette + paletteOffset) );
	assert(paletteIndex < kNumPaletteColors);
	const Color4& color = g_paletteColors[paletteIndex];

	m_renderer->DrawPixel(x, y, color);
}
