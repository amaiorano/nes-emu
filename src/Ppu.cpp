#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"
#include "MemoryMap.h"
#include "Debugger.h"
#include <tuple>
#include <cstring>

namespace
{
	const size_t kScreenWidth = 256;
	const size_t kScreenHeight = 240;

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

	template <typename T, typename U>
	bool IncAndWrap(T& v, U size)
	{
		if (++v == size)
		{
			v = 0;
			return true;
		}
		return false;
	}

	FORCEINLINE uint32 YXtoPpuCycle(uint32 y, uint32 x)
	{
		return y * 341 + x;
	}

	FORCEINLINE uint32 CpuToPpuCycles(uint32 cpuCycles)
	{
		return cpuCycles * 3;
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
		SpriteOverflow		= BIT(5),
		PpuHitSprite0		= BIT(6),
		InVBlank			= BIT(7),
	};
}

Ppu::Ppu()
	: m_ppuMemoryBus(nullptr)
	, m_nes(nullptr)
	, m_rendererHolder(new Renderer())
	, m_renderer(m_rendererHolder.get())
{
	InitPaletteColors();
	m_renderer->Create(kScreenWidth, kScreenHeight);
}

void Ppu::Initialize(PpuMemoryBus& ppuMemoryBus, Nes& nes)
{
	m_ppuMemoryBus = &ppuMemoryBus;
	m_nes = &nes;

	m_nameTables.Initialize();
	m_palette.Initialize();
	m_ppuRegisters.Initialize();
	m_oam.Initialize();
	m_oam2.Initialize();

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

	m_numSpritesToRender = 0;

	m_cycle = 0;
	m_evenFrame = true;
	m_vblankFlagSetThisFrame = false;
}

void Ppu::Serialize(class Serializer& serializer)
{
	SERIALIZE(m_nameTables);
	SERIALIZE(m_palette);
	SERIALIZE(m_oam);
	SERIALIZE(m_oam2);
	SERIALIZE(m_ppuRegisters);
	SERIALIZE(m_vramAndScrollFirstWrite);
	SERIALIZE(m_vramAddress);
	SERIALIZE(m_tempVRamAddress);
	SERIALIZE(m_fineX);
	SERIALIZE(m_vramBufferedValue);
	SERIALIZE(m_cycle);
	SERIALIZE(m_evenFrame);
	SERIALIZE(m_vblankFlagSetThisFrame);
	SERIALIZE(m_bgTileFetchDataPipeline);
	SERIALIZE(m_spriteFetchData);
}

void Ppu::Execute(uint32 cpuCycles, bool& completedFrame)
{
	const size_t kNumTotalScanlines = 262;
	const size_t kNumHBlankAndBorderCycles = 85;
	const size_t kNumScanlineCycles = kScreenWidth + kNumHBlankAndBorderCycles; // 256 + 85 = 341
	const size_t kNumScreenCycles = kNumScanlineCycles * kNumTotalScanlines; // 89342 cycles per screen

	uint32 ppuCycles = CpuToPpuCycles(cpuCycles);

	completedFrame = false;

	const bool renderingEnabled = m_ppuControlReg2->Test(PpuControl2::RenderBackground|PpuControl2::RenderSprites);

	for ( ; ppuCycles > 0; --ppuCycles)
	{
		const uint32 x = m_cycle % kNumScanlineCycles; // offset in current scanline
		const uint32 y = m_cycle / kNumScanlineCycles; // scanline

		if ( (y <= 239) || y == 261 ) // Visible and Pre-render scanlines
		{
			if (renderingEnabled) //@TODO: Not sure about this
			{
				if (x == 64)
				{
					// Cycles 1-64: Clear secondary OAM to $FF
					ClearOAM2();
				}
				else if (x == 256)
				{
					// Cycles 65-256: Sprite evaluation
					PerformSpriteEvaluation(x, y);
				}
				else if (x == 260)
				{
					//@TODO: This is a dirty hack for Mapper4 (MMC3) and the like to get around the fact that
					// my PPU implementation doesn't perform Sprite fetches as expected (must fetch even if no
					// sprites found on scanline, and fetch each sprite separately like I do for tiles). For now
					// this mostly works.
					m_nes->HACK_OnScanline();
				}
			}

			if (x >= 257 && x <= 320) // "HBlank" (idle cycles)
			{
				if (renderingEnabled)
				{
					if (x == 257)
					{
						CopyVRamAddressHori(m_vramAddress, m_tempVRamAddress);
					}
					else if (y == 261 && x >= 280 && x <= 304)
					{
						//@TODO: could optimize by just doing this once on last cycle (x==304)
						CopyVRamAddressVert(m_vramAddress, m_tempVRamAddress);
					}
					else if (x == 320)
					{
						// Cycles 257-320: sprite data fetch for next scanline
						FetchSpriteData(y);
					}
				}
			}
			else // Fetch and render cycles
			{
				assert(x <= 256 || (x >= 321 && x <= 340));

				// Update VRAM address and fetch data
				if (renderingEnabled)
				{
					// PPU fetches 4 bytes every 8 cycles for a given tile (NT, AT, LowBG, and HighBG).
					// We want to know when we're on the last cycle of the HighBG tile byte (see Ntsc_timing.jpg)
					const bool lastFetchCycle = (x >= 8) && (x % 8 == 0);

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
				}

				// Render pixel at x,y using pipelined fetch data. If rendering is disabled, will render background color.
				if (x < kScreenWidth && y < kScreenHeight)
				{
					RenderPixel(x, y);
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
					completedFrame = true;
					OnFrameComplete();
				}
			}
		}
		else // Post-render and VBlank 240-260
		{
			assert(y >= 240 && y <= 260);

			if (y == 241 && x == 1)
			{
				SetVBlankFlag();

				if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
					m_nes->SignalCpuNmi();
			}
		}

		// Update cycle
		m_cycle = (m_cycle + 1) % kNumScreenCycles;
	}
}

void Ppu::RenderFrame()
{
	m_renderer->Present();
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
			//@HACK: Some games like Bomberman and Burger Time poll $2002.7 (VBlank flag) expecting the
			// bit to be set before the NMI executes. On actual hardware, this results in a race condition
			// where sometimes the bit won't be set, or the NMI won't occur. See:
			// http://wiki.nesdev.com/w/index.php/NMI#Race_condition and
			// http://forums.nesdev.com/viewtopic.php?t=5005
			// In my emulator code, CPU and PPU execute sequentially, so this race condition does not occur;
			// instead, $2002.7 will normally never be set before NMI is processed, breaking games that
			// depend on it. To fix this, we assume the CPU instruction that executed this read will be at
			// least 3 CPU cycles long, and we check if we _will_ set the VBlank flag on the next PPU update;
			// if so, we set the flag right away and return it.
			const uint32 kSetVBlankCycle = YXtoPpuCycle(241, 1);
			if (m_cycle < kSetVBlankCycle && (m_cycle + CpuToPpuCycles(3) >= kSetVBlankCycle))
			{
				SetVBlankFlag();
			}

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
			const bool enabledNmiOnVBlank = !oldPpuControlReg1->Test(PpuControl1::NmiOnVBlank) && m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank);
			if ( enabledNmiOnVBlank && m_ppuStatusReg->Test(PpuStatus::InVBlank) ) // In vblank (and $2002 not read yet, which resets this bit)
			{
				m_nes->SignalCpuNmi();
			}
		}
		break;

	case CpuMemory::kPpuSprRamIoReg: // $2004
		{
			// Write value to sprite ram at address in $2003 (OAMADDR) and increment address
			const uint8 spriteRamAddress = ReadPpuRegister(CpuMemory::kPpuSprRamAddressReg);
			m_oam.Write(spriteRamAddress, value);
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
	switch (m_nes->GetNameTableMirroring())
	{
	case NameTableMirroring::Vertical:
		// Vertical mirroring (horizontal scrolling)
		// A B
		// A B
		// Simplest case, just wrap >= 2K
		physicalVRamAddress = virtualVRamAddress % NameTableMemory::kSize;
		break;

	case NameTableMirroring::Horizontal:
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
		break;

	case NameTableMirroring::OneScreenUpper:
		// A A
		// A A
		physicalVRamAddress = virtualVRamAddress % (NameTableMemory::kSize / 2);
		break;

	case NameTableMirroring::OneScreenLower:
		// B B
		// B B
		physicalVRamAddress = (virtualVRamAddress % (NameTableMemory::kSize / 2)) + (NameTableMemory::kSize / 2);
		break;

	default:
		assert(false);
		break;
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
	nextTile.bmpLow = m_ppuMemoryBus->Read(byte1Address);
	nextTile.bmpHigh = m_ppuMemoryBus->Read(byte2Address);
	nextTile.paletteHighBits = paletteHighBits;

#if CONFIG_DEBUG
	auto& nextTile_DEBUG = m_bgTileFetchDataPipeline_DEBUG[1];
	nextTile_DEBUG.vramAddress = m_vramAddress;
	nextTile_DEBUG.tileIndexAddress = tileIndexAddress;
	nextTile_DEBUG.attributeAddress = attributeAddress;
	nextTile_DEBUG.attributeShift = attributeShift;
	nextTile_DEBUG.byte1Address = byte1Address;
#endif
}

void Ppu::ClearOAM2() // OAM2 = $FF
{
	//@NOTE: We don't actually need this step as we track number of sprites to render per scanline
	memset(m_oam2.RawPtr(), 0xFF, m_oam2.Size());
}

void Ppu::PerformSpriteEvaluation(uint32 /*x*/, uint32 y) // OAM -> OAM2
{
	// See http://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation

	static auto IsSpriteInRangeY = [] (uint32 y, uint8 spriteY, uint8 spriteHeight) -> bool
	{
		return (y >= spriteY && y < static_cast<uint8>(spriteY + spriteHeight) && spriteY < kScreenHeight);
	};

	const bool isSprite8x16 = m_ppuControlReg1->Test(PpuControl1::SpriteSize8x16);
	const uint8 spriteHeight = isSprite8x16? 16 : 8;
	
	// Reset sprite vars for current scanline
	m_numSpritesToRender = 0;
	m_renderSprite0 = false;

	uint16 n = 0; // Sprite [0-63] in OAM
	auto& n2 = m_numSpritesToRender; // Sprite [0-7] in OAM2

	typedef uint8 SpriteData[4]; //@TODO: Maybe we should just store m_oam and m_oam2 as arrays of this
	SpriteData* oam = m_oam.RawPtrAs<SpriteData*>();
	SpriteData* oam2 = m_oam2.RawPtrAs<SpriteData*>();

	// Attempt to find up to 8 sprites on current scanline
	while (n2 < 8)
	{
		const uint8 spriteY = oam[n][0];
		oam2[n2][0] = spriteY; // (1)

		if (IsSpriteInRangeY(y, spriteY, spriteHeight)) // (1a)
		{
			oam2[n2][1] = oam[n][1];
			oam2[n2][2] = oam[n][2];
			oam2[n2][3] = oam[n][3];

			if (n == 0) // If we're going to render sprite 0, set flag so we can detect sprite 0 hit when we render
			{
				m_renderSprite0 = true;
			}

			++n2;
		}

		++n; // (2)
		if (n == 64) // (2a)
		{
			// We didn't find 8 sprites, OAM2 contains what we've found so far, so we can bail
			return;
		}
	}

	// We found 8 sprites above. Let's see if there are any more so we can set sprite overflow flag.
	uint16 m = 0; // Byte in sprite data [0-3]
	
	while (n < 64)
	{
		const uint8 spriteY = oam[n][m]; // (3) Evaluate OAM[n][m] as a Y-coordinate (it might not be)
		IncAndWrap(m, 4);
		
		if (IsSpriteInRangeY(y, spriteY, spriteHeight)) // (3a)
		{
			m_ppuStatusReg->Set(PpuStatus::SpriteOverflow);

			// PPU reads next 3 bytes from OAM. Because of the hardware bug (below), m might not be 1 here, so
			// we carefully increment n when m overflows.
			for (int i = 0; i < 3; ++i)
			{
				if (IncAndWrap(m, 4))
				{
					++n;
				}
			}
		}
		else
		{
			// (3b) If the value is not in range, increment n and m (without carry). If n overflows to 0, go to 4; otherwise go to 3
			// The m increment is a hardware bug - if only n was incremented, the overflow flag would be set whenever more than 8
			// sprites were present on the same scanline, as expected.
			++n;
			
			IncAndWrap(m, 4); // This increment is a hardware bug
		}
	}
}

void Ppu::FetchSpriteData(uint32 y) // OAM2 -> render (shift) registers
{
	// See http://wiki.nesdev.com/w/index.php/PPU_rendering#Cycles_257-320

	static auto FlipBits = [] (uint8 v) -> uint8
	{
		return 
			  ((v & BIT(0)) << 7)
			| ((v & BIT(1)) << 5)
			| ((v & BIT(2)) << 3)
			| ((v & BIT(3)) << 1)
			| ((v & BIT(4)) >> 1)
			| ((v & BIT(5)) >> 3)
			| ((v & BIT(6)) >> 5)
			| ((v & BIT(7)) >> 7);
	};

	typedef uint8 SpriteData[4];
	SpriteData* oam2 = m_oam2.RawPtrAs<SpriteData*>();

	const bool isSprite8x16 = m_ppuControlReg1->Test(PpuControl1::SpriteSize8x16);

	for (uint8 n = 0; n < m_numSpritesToRender; ++n)
	{
		const uint8 spriteY = oam2[n][0];
		const uint8 byte1 = oam2[n][1];
		const uint8 attribs = oam2[n][2];
		const bool flipHorz = TestBits(attribs, BIT(6));
		const bool flipVert = TestBits(attribs, BIT(7));

		uint16 patternTableAddress;
		uint8 tileIndex;
		if ( !isSprite8x16 ) // 8x8 sprite, oam byte 1 is tile index
		{
			patternTableAddress = m_ppuControlReg1->Test(PpuControl1::SpritePatternTableAddress8x8)? 0x1000 : 0x0000;
			tileIndex = byte1;
		}
		else // 8x16 sprite, both address and tile index are stored in oam byte 1
		{
			patternTableAddress = TestBits(byte1, BIT(0))? 0x1000 : 0x0000;
			tileIndex = ReadBits(byte1, ~BIT(0));
		}

		uint8 yOffset = static_cast<uint8>(y) - spriteY;
		assert(yOffset < (isSprite8x16? 16 : 8));

		if (isSprite8x16)
		{
			// In 8x16 mode, first tile is at tileIndex, second tile (underneath) is at tileIndex + 1
			uint8 nextTile = 0;
			if (yOffset >= 8)
			{
				++nextTile;
				yOffset -= 8;
			}

			// In 8x16 mode, vertical flip also flips the tile index order
			if (flipVert)
			{
				nextTile = (nextTile + 1) % 2;
			}

			tileIndex += nextTile;
		}

		if (flipVert)
		{
			yOffset = 7 - yOffset;
		}
		assert(yOffset < 8);
		
		const uint16 tileOffset = TO16(tileIndex) * 16;
		const uint16 byte1Address = patternTableAddress + tileOffset + yOffset;
		const uint16 byte2Address = byte1Address + 8;

		auto& data = m_spriteFetchData[n];
		data.bmpLow = m_ppuMemoryBus->Read(byte1Address);
		data.bmpHigh = m_ppuMemoryBus->Read(byte2Address);
		data.attributes = oam2[n][2];
		data.x = oam2[n][3];

		if (flipHorz)
		{
			data.bmpLow = FlipBits(data.bmpLow);
			data.bmpHigh = FlipBits(data.bmpHigh);
		}
	}
}

void Ppu::RenderPixel(uint32 x, uint32 y)
{
	// See http://wiki.nesdev.com/w/index.php/PPU_rendering

	static auto GetBackgroundColor = [&] (Color4& color)
	{
		color = g_paletteColors[m_palette.Read(0)]; // BG ($3F00)
	};

	static auto GetPaletteColor = [&] (uint8 highBits, uint8 lowBits, uint16 paletteBaseAddress, Color4& color)
	{
		assert(lowBits != 0);

		// Compute offset into palette memory that contains the palette index
		const uint8 paletteOffset = (highBits << 2) | (lowBits & 0x3);

		//@NOTE: lowBits is never 0, so we don't have to worry about mapping every 4th byte to 0 (bg color) here.
		// That case is handled specially in the multiplexer code.
		const uint8 paletteIndex = m_palette.Read( MapPpuToPalette(paletteBaseAddress + paletteOffset) );
		color = g_paletteColors[paletteIndex & (kNumPaletteColors-1)]; // Mask in only required bits, some roms write values > 64
	};

	bool bgRenderingEnabled = m_ppuControlReg2->Test(PpuControl2::RenderBackground);
	bool spriteRenderingEnabled = m_ppuControlReg2->Test(PpuControl2::RenderSprites);
	
	// Consider bg/sprites as disabled (for this pixel) if we're not supposed to render it in the left-most 8 pixels
	if ( !m_ppuControlReg2->Test(PpuControl2::BackgroundShowLeft8) && x < 8 )
	{
		bgRenderingEnabled = false;
	}

	if ( !m_ppuControlReg2->Test(PpuControl2::SpritesShowLeft8) && x < 8 )
	{
		spriteRenderingEnabled = false;
	}
	
	// Get the background pixel
	uint8 bgPaletteHighBits = 0;
	uint8 bgPaletteLowBits = 0;
	if (bgRenderingEnabled)
	{
		// At this point, the data for the current and next tile are in m_bgTileFetchDataPipeline
		const auto& currTile = m_bgTileFetchDataPipeline[0];
		const auto& nextTile = m_bgTileFetchDataPipeline[1];

		// Mux uses fine X to select a bit from shift registers
		const uint16 muxMask = 1 << (7 - m_fineX);

		// Instead of actually shifting every cycle, we rebuild the shift register values
		// for the current cycle (using the x value)
		//@TODO: Optimize by storing 16 bit values for low and high bitmap bytes and shifting every cycle
		const uint8 xShift = x % 8;
		const uint8 shiftRegLow = (currTile.bmpLow << xShift) | (nextTile.bmpLow >> (8 - xShift));
		const uint8 shiftRegHigh = (currTile.bmpHigh << xShift) | (nextTile.bmpHigh >> (8 - xShift));

		bgPaletteLowBits = (TestBits01(shiftRegHigh, muxMask) << 1) | (TestBits01(shiftRegLow, muxMask));

		// Technically, the mux would index 2 8-bit registers containing replicated values for the current
		// and next tile palette high bits (from attribute bytes), but this is faster.
		bgPaletteHighBits = (xShift + m_fineX < 8)? currTile.paletteHighBits : nextTile.paletteHighBits;
	}

	// Get the potential sprite pixel
	bool foundSprite = false;
	bool spriteHasBgPriority = false;
	bool isSprite0 = false;
	uint8 sprPaletteHighBits = 0;
	uint8 sprPaletteLowBits = 0;
	if (spriteRenderingEnabled)
	{
		for (uint8 n = 0; n < m_numSpritesToRender; ++n)
		{
			auto& spriteData = m_spriteFetchData[n];

			if ( (x >= spriteData.x) && (x < (spriteData.x + 8u)) )
			{
				if (!foundSprite)
				{
					// Compose "sprite color" (0-3) from high bit in bitmap bytes
					sprPaletteLowBits = (TestBits01(spriteData.bmpHigh, 0x80) << 1) | (TestBits01(spriteData.bmpLow, 0x80));

					// First non-transparent pixel moves on to multiplexer
					if (sprPaletteLowBits != 0)
					{
						foundSprite = true;
						sprPaletteHighBits = ReadBits(spriteData.attributes, 0x3); //@TODO: cache this in spriteData
						spriteHasBgPriority = TestBits(spriteData.attributes, BIT(5));

						if (m_renderSprite0 && (n == 0)) // Rendering pixel from sprite 0?
						{
							isSprite0 = true;
						}
					}
				}

				// Shift out high bits - do this for all (overlapping) sprites in range
				spriteData.bmpLow <<= 1;
				spriteData.bmpHigh <<= 1;
			}
		}
	}

	// Multiplexer selects background or sprite pixel (see "Priority multiplexer decision table")
	Color4 color;

	if (bgPaletteLowBits == 0)
	{
		if (!foundSprite || sprPaletteLowBits == 0)
		{
			// Background color 0
			GetBackgroundColor(color);
		}
		else
		{
			// Sprite color
			GetPaletteColor(sprPaletteHighBits, sprPaletteLowBits, PpuMemory::kSpritePalette, color);
		}
	}
	else
	{
		if (foundSprite && !spriteHasBgPriority)
		{
			// Sprite color
			GetPaletteColor(sprPaletteHighBits, sprPaletteLowBits, PpuMemory::kSpritePalette, color);
		}
		else
		{
			// BG color
			GetPaletteColor(bgPaletteHighBits, bgPaletteLowBits, PpuMemory::kImagePalette, color);
		}

		if (isSprite0)
		{
			m_ppuStatusReg->Set(PpuStatus::PpuHitSprite0);
		}
	}

	m_renderer->DrawPixel(x, y, color);
}

void Ppu::SetVBlankFlag()
{
	if (!m_vblankFlagSetThisFrame)
	{
		m_ppuStatusReg->Set(PpuStatus::InVBlank);
		m_vblankFlagSetThisFrame = true;
	}
}

void Ppu::OnFrameComplete()
{
	const bool renderingEnabled = m_ppuControlReg2->Test(PpuControl2::RenderBackground|PpuControl2::RenderSprites);
	
	// For odd frames, the cycle at the end of the scanline (340,239) is skipped
	if (!m_evenFrame && renderingEnabled)
		++m_cycle;

	m_evenFrame = !m_evenFrame;
	m_vblankFlagSetThisFrame = false;
}
