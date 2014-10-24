#include "Ppu.h"
#include "Memory.h" //@TODO: Rename CpuRam.h to Memory.h
#include "Renderer.h"

Ppu::Ppu()
	: m_cpuRam(nullptr)
	, m_ppuRam(nullptr)
	, m_spriteRam(nullptr)
{
}

void Ppu::Initialize(CpuRam& cpuRam, PpuRam& ppuRam, SpriteRam& spriteRam)
{
	m_cpuRam = &cpuRam;
	m_ppuRam = &ppuRam;
	m_spriteRam = &spriteRam;
}

void Ppu::Reset()
{
}

void Ppu::Run()
{
	struct Bits8
	{
		Bits8() {}
		Bits8(uint8 value) : value(value) {}

		union
		{
			struct
			{
				//@TODO: ifdef LITTLE_ENDIAN
				uint8 Bit0 : 1;
				uint8 Bit1 : 1;
				uint8 Bit2 : 1;
				uint8 Bit3 : 1;
				uint8 Bit4 : 1;
				uint8 Bit5 : 1;
				uint8 Bit6 : 1;
				uint8 Bit7 : 1;
			};
			uint8 value;
		};

	};
	static_assert(sizeof(Bits8)==1, "Invalid size");

	Renderer renderer;
	renderer.Create();

	const uint8* chrRom = m_ppuRam->UnsafePtr(0);

	static auto ReadTile = [&chrRom] (int tileIndex, uint8 tile[8][8])
	{
		// Every 16 bytes is 1 8x8 tile
		const int tileOffset = tileIndex * 16;
		assert(tileOffset+16<KB(16));
		Bits8* pByte1 = (Bits8*)&chrRom[tileOffset + 0];
		Bits8* pByte2 = (Bits8*)&chrRom[tileOffset + 8];

		for (size_t y = 0; y < 8; ++y)
		{
			tile[0][y] = pByte1->Bit7 | (pByte2->Bit7 << 1);
			tile[1][y] = pByte1->Bit6 | (pByte2->Bit6 << 1);
			tile[2][y] = pByte1->Bit5 | (pByte2->Bit5 << 1);
			tile[3][y] = pByte1->Bit4 | (pByte2->Bit4 << 1);
			tile[4][y] = pByte1->Bit3 | (pByte2->Bit3 << 1);
			tile[5][y] = pByte1->Bit2 | (pByte2->Bit2 << 1);
			tile[6][y] = pByte1->Bit1 | (pByte2->Bit1 << 1);
			tile[7][y] = pByte1->Bit0 | (pByte2->Bit0 << 1);

			++pByte1;
			++pByte2;
		}
	};

	static auto DrawTile = [&renderer] (int x, int y, uint8 tile[8][8])
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

	for ( ; ; )
	{
		renderer.Clear();

		// Render Pattern Table #0: [$0000,$1000[ (4 kb) = 256 tiles
		// Render Pattern Table #1: [$1000,$2000[ (4 kb) = 256 tiles

		int tileIndex = 0;
		uint8 tile[8][8] = {0};
		for (int y = 0; y < 32; ++y)
		{
			for (int x = 0; x < 16; ++x)
			{
				ReadTile(tileIndex, tile);
				DrawTile(x*8, y*8, tile);
				++tileIndex;
			}
		}

		renderer.Render();
	}
}