#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"

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

	m_ppuControlReg1	= m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControlReg1);
	m_ppuControlReg2	= m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControlReg2);
	m_ppuStatusReg		= m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuStatusReg);
	m_sprRamAddressReg	= m_cpuRam->UnsafePtr(CpuRam::kPpuSprRamAddressReg);
	m_sprRamIoReg		= m_cpuRam->UnsafePtr(CpuRam::kPpuSprRamIoReg);
	m_vramAddressReg1	= m_cpuRam->UnsafePtr(CpuRam::kPpuVRamAddressReg1);
	m_vramAddressReg2	= m_cpuRam->UnsafePtr(CpuRam::kPpuVRamAddressReg2);
	m_vramIoReg			= m_cpuRam->UnsafePtr(CpuRam::kPpuVRamIoReg);
}

void Ppu::Reset()
{
	// Fake a first vblank
	m_ppuStatusReg->Set(PpuStatus::InVBlank);

	m_vramAddressReg2High = true;
	m_vramAddress = 0xDDDD;
	m_vramBufferedValue = 0xDD;
}

void Ppu::Run()
{
	static bool startRendering = false;

	if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
	{
		//@HACK: For now render every 10th frame while NMI is enabled (this is wrong, we should always render)
		static int count = 1;
		if (--count == 0)
		{
			//printf("NmiOnVBlank allowed! Signalling NMI\n");
			count = 50;
			m_nes->SignalCpuNmi();
			startRendering = true;

			m_ppuStatusReg->Set(PpuStatus::InVBlank);
			Render();
			m_ppuStatusReg->Clear(PpuStatus::InVBlank);
		}
	}

	//if (startRendering)
	//	Render();
}

void Ppu::OnCpuMemoryPreRead(uint16 address)
{
	switch (address)
	{
		case CpuRam::kPpuVRamIoReg: // $2007
			assert(m_vramIoReg == m_cpuRam->UnsafePtr(address));
			assert(m_vramAddressReg2High && "User code error: trying to read from $2007 when VRAM address not yet set correct via $2006");
			
			// About to read $2007, so update it from buffered value in CIRAM or palette RAM value
			*m_vramIoReg = m_vramAddress < PpuRam::kImagePalette? m_vramBufferedValue : m_cpuRam->Read8(m_vramAddress);

			// Update buffered value with current vram value
			m_vramBufferedValue = m_cpuRam->Read8( PpuRam::kNameTable0 + (m_vramAddress & (KB(4)-1)) ); // Wrap into 4K address space of CIRAM

			// Advance VRAM pointer
			m_vramAddress += PpuControl1::GetPpuAddressIncrementSize( m_ppuControlReg1->Value() );

			break;
	}
}

void Ppu::OnCpuMemoryPostRead(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuStatusReg: // $2002
		m_ppuStatusReg->Clear(PpuStatus::InVBlank);
		//@TODO: After a read occurs, $2005 is reset, hence the next write to $2005 will be Horizontal.

		// After a read occurs, $2006 is reset, hence the next write to $2006 will be the high byte portion.
		m_vramAddressReg2High = true;
		break;
	}
}

void Ppu::OnCpuMemoryPostWrite(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuVRamAddressReg2: // $2006
		{
			// Implement double write (alternate between writing high byte and low byte)
			assert(m_vramAddressReg2 == m_cpuRam->UnsafePtr(address));
			const uint16 halfAddress = TO16(*m_vramAddressReg2);
			if (m_vramAddressReg2High)
				m_vramAddress = (halfAddress << 8) | (m_vramAddress & 0x00FF);
			else
				m_vramAddress = halfAddress | (m_vramAddress & 0xFF00);

			m_vramAddressReg2High = !m_vramAddressReg2High; // Toggle high/low
		}
		break;

	case CpuRam::kPpuVRamIoReg: // $2007
		{
			// Write value to VRAM and advance VRAM pointer
			assert(m_vramIoReg == m_cpuRam->UnsafePtr(address));
			uint8 value = *m_vramIoReg;
			m_ppuRam->Write8(m_vramAddress, value);
			m_vramAddress += PpuControl1::GetPpuAddressIncrementSize( m_ppuControlReg1->Value() );
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
	
	const uint16 currBgPatternTableAddress = PpuControl1::GetBackgroundPatternTableAddress(m_ppuControlReg1->Value());
	const uint8* patternTable = m_ppuRam->UnsafePtr(currBgPatternTableAddress);

	const uint16 currNameTableAddress = PpuControl1::GetNameTableAddress(m_ppuControlReg1->Value());
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
