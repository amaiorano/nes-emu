#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"

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

Ppu::Ppu()
	: m_renderer(new Renderer())
	, m_cpuRam(nullptr)
	, m_ppuRam(nullptr)
	, m_spriteRam(nullptr)
{
	m_renderer->Create();
}

void Ppu::Initialize(Nes& nes, CpuRam& cpuRam, PpuRam& ppuRam, SpriteRam& spriteRam)
{
	m_nes = &nes;
	m_cpuRam = &cpuRam;
	m_ppuRam = &ppuRam;
	m_spriteRam = &spriteRam;

	m_ppuControl1 = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControl1);
	m_ppuControl2 = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControl2);
	m_ppuStatus = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuStatus);
}

void Ppu::Reset()
{
	// Fake a first vblank
	m_ppuStatus->Set(PpuStatus::InVBlank);
}

void Ppu::Run()
{
	static bool startRendering = false;

	if (m_ppuControl1->Test(PpuControl1::NmiOnVBlank))
	{
		//@HACK: For now render every 10th frame while NMI is enabled (this is wrong, we should always render)
		static int count = 1;
		if (--count == 0)
		{
			//printf("NmiOnVBlank allowed! Signalling NMI\n");
			count = 50;
			m_nes->SignalCpuNmi();
			startRendering = true;

			m_ppuStatus->Set(PpuStatus::InVBlank);
			Render();
			m_ppuStatus->Clear(PpuStatus::InVBlank);
		}
	}

	//if (startRendering)
	//	Render();
}

void Ppu::OnCpuMemoryRead(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuStatus: // $2002
		m_ppuStatus->Clear(PpuStatus::InVBlank);
		//@TODO: After a read occurs, $2005 is reset, hence the next write to $2005 will be Horizontal.

		// After a read occurs, $2006 is reset, hence the next write to $2006 will be the high byte portion.
		m_vramAddress.high = true;

		break;
	}
}

void Ppu::OnCpuMemoryWrite(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuVRamAddress2: // $2006
		{
			// Implement double write (alternate between writing high byte and low byte)
			uint16 value = TO16(m_cpuRam->Read8(address));
			if (m_vramAddress.high)
				m_vramAddress.address = (value << 8) | (m_vramAddress.address & 0x00FF);
			else
				m_vramAddress.address = value | (m_vramAddress.address & 0xFF00);
			
			m_vramAddress.high = !m_vramAddress.high;
		}
		break;

	case CpuRam::kPpuVRamIO: // $2007
		{
			// Write value to VRAM and increment address
			uint8 value = m_cpuRam->Read8(address);
			m_ppuRam->Write8(m_vramAddress.address, value);
			m_vramAddress.address += PpuControl1::GetPpuAddressIncrementSize( m_ppuControl1->Value() );
		}
		break;
	}
}

void Ppu::Render()
{
	static auto ReadTile = [] (const uint8* patternTable, int tileIndex, uint8 tile[8][8])
	{
		// Every 16 bytes is 1 8x8 tile
		const int tileOffset = tileIndex * 16;
		assert(tileOffset+16 < KB(16));
		Bitfield8* pByte1 = (Bitfield8*)&patternTable[tileOffset + 0];
		Bitfield8* pByte2 = (Bitfield8*)&patternTable[tileOffset + 8];

		for (size_t y = 0; y < 8; ++y)
		{
			tile[0][y] = pByte1->TestPos(7) | (pByte2->TestPos(7) << 1);
			tile[1][y] = pByte1->TestPos(6) | (pByte2->TestPos(6) << 1);
			tile[2][y] = pByte1->TestPos(5) | (pByte2->TestPos(5) << 1);
			tile[3][y] = pByte1->TestPos(4) | (pByte2->TestPos(4) << 1);
			tile[4][y] = pByte1->TestPos(3) | (pByte2->TestPos(3) << 1);
			tile[5][y] = pByte1->TestPos(2) | (pByte2->TestPos(2) << 1);
			tile[6][y] = pByte1->TestPos(1) | (pByte2->TestPos(1) << 1);
			tile[7][y] = pByte1->TestPos(0) | (pByte2->TestPos(0) << 1);

			++pByte1;
			++pByte2;
		}
	};

	static auto DrawTile = [] (Renderer& renderer, int x, int y, uint8 tile[8][8])
	{
		Color4 color;

		for (size_t ty = 0; ty < 8; ++ty)
		{
			for (size_t tx = 0; tx < 8; ++tx)
			{
				switch (tile[tx][ty])
				{
				case 0: color = Color4::Black(); break;
				case 1: color = Color4::Blue(); break;
				case 2: color = Color4::Red(); break;
				case 3: color = Color4::Green(); break;
				default: assert(false); break;
				}

				renderer.DrawPixel(x + tx, y + ty, color);
			}
		}
	};
	
	const uint16 currBgPatternTableAddress = PpuControl1::GetBackgroundPatternTableAddress(m_ppuControl1->Value());
	const uint8* patternTable = m_ppuRam->UnsafePtr(currBgPatternTableAddress);

	const uint16 currNameTableAddress = PpuControl1::GetNameTableAddress(m_ppuControl1->Value());
	const uint8* nameTable = m_ppuRam->UnsafePtr(currNameTableAddress); 

	m_renderer->Clear();

	uint8 tile[8][8] = {0};
	for (int y = 0; y < 30; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			int tileIndex = nameTable[y*32 + x];
			ReadTile(patternTable, tileIndex, tile);
			DrawTile(*m_renderer, x*8, y*8, tile);
		}
	}

	m_renderer->Render();
}
