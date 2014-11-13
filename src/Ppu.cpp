#include "Ppu.h"
#include "Nes.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"
#include "Debugger.h"

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
	: m_ppuMemoryBus(nullptr)
	, m_nes(nullptr)
	, m_renderer(new Renderer())
{
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
	// Fake a first vblank
	m_ppuStatusReg->Set(PpuStatus::InVBlank);

	m_vramAddressHigh = true;
	m_vramAddress = 0xDDDD;
	m_vramBufferedValue = 0xDD;
}

void Ppu::Run()
{
	enum State { InitialVBlanks, WaitForInitialNmiEnable, Rendering, VBlanking };

	static State currState = InitialVBlanks;

	const int NumCpuInstructionsForRender = 700;
	const int NumCpuInstructionsForVBlank = (int)(NumCpuInstructionsForRender * 0.05f);

	switch (currState)
	{
	case InitialVBlanks:
		{
			m_renderer->Clear();
			m_renderer->Render();

			static int frames = 4;
			if (frames == 4 || frames == 2)
				m_ppuStatusReg->Clear(PpuStatus::InVBlank);
			else
				m_ppuStatusReg->Set(PpuStatus::InVBlank);

			if (--frames == 0)
			{
				currState = VBlanking;
			}
		}
		break;

	case WaitForInitialNmiEnable:
		m_renderer->Clear();
		m_renderer->Render();

		if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
			currState = VBlanking;
		break;

	case Rendering:
		{
			static int numCpuInstructions = NumCpuInstructionsForRender;
			if (--numCpuInstructions > 0)
				break;
			numCpuInstructions = NumCpuInstructionsForRender;


			if (m_ppuControlReg2->Test(PpuControl2::BackgroundVisible))
				Render();

			currState = VBlanking;
			// VBlanking::OnEnter
			m_ppuStatusReg->Set(PpuStatus::InVBlank);
			if (m_ppuControlReg1->Test(PpuControl1::NmiOnVBlank))
				m_nes->SignalCpuNmi();
		}
		break;

	case VBlanking:
		{
			m_renderer->Render();

			static int numCpuInstructions = NumCpuInstructionsForVBlank;
			if (--numCpuInstructions > 0)
				break;
			numCpuInstructions = NumCpuInstructionsForVBlank;

			currState = Rendering;
			// Rendering::OnEnter
			m_ppuStatusReg->Clear(PpuStatus::InVBlank);
		}
		break;
	}
}

uint8 Ppu::HandleCpuRead(uint16 cpuAddress)
{
	// CPU only has access to PPU memory-mapped registers
	const uint16 ppuRegisterAddress = MapCpuToPpuRegister(cpuAddress);

	// If debugger is reading, we don't want any register side-effects, so just return the value
	if ( Debugger::IsExecuting() )
	{
		return m_ppuRegisters.Read(ppuRegisterAddress);
	}
	
	switch (cpuAddress)
	{
	case CpuMemory::kPpuVRamIoReg: // $2007
		{
			assert(m_vramAddressHigh && "User code error: trying to read from $2007 when VRAM address not yet fully set via $2006");

			// Read from palette or return buffered value
			if (m_vramAddress >= PpuMemory::kPalettesBase)			
			{
				m_ppuRegisters.Write(ppuRegisterAddress, m_palette.Read(MapPpuToPalette(m_vramAddress)));
			}
			else
			{
				m_ppuRegisters.Write(ppuRegisterAddress, m_vramBufferedValue);
			}

			// Always update buffered value from current vram pointer before incrementing it.
			// Note that we don't buffer palette values, we read "under it", which mirrors the name table memory (VRAM/CIRAM).
			m_vramBufferedValue = m_ppuMemoryBus->Read(m_vramAddress);
				
			// Advance vram pointer
			const uint16 advanceOffset = PpuControl1::GetPpuAddressIncrementSize( m_ppuControlReg1->Value() );				
			m_vramAddress += advanceOffset;
		}
	}

	return m_ppuRegisters.Read(ppuRegisterAddress);
}

void Ppu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	switch (cpuAddress)
	{
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

	// Update register value
	m_ppuRegisters.Write(MapCpuToPpuRegister(cpuAddress), value);
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
	return (ppuAddress - PpuMemory::kPalettesBase) % PpuMemory::kPalettesSize;
}

void Ppu::Render()
{
	static auto ReadTile = [&] (uint16 patternTableAddress, uint8 tileIndex, uint8 tile[8][8])
	{
		// Every 16 bytes is 1 8x8 tile
		const uint16 tileOffset = TO16(tileIndex) * 16;
		assert(tileOffset+16 < KB(16));

		uint16 byte1Address = patternTableAddress + tileOffset + 0;
		uint16 byte2Address = patternTableAddress + tileOffset + 8;

		for (uint16 y = 0; y < 8; ++y)
		{
			const uint8 b1 = m_ppuMemoryBus->Read(byte1Address);
			const uint8 b2 = m_ppuMemoryBus->Read(byte2Address);
			const Bitfield8& byte1 = reinterpret_cast<const Bitfield8&>(b1);
			const Bitfield8& byte2 = reinterpret_cast<const Bitfield8&>(b2);

			tile[0][y] = byte1.TestPos(7) | (byte2.TestPos(7) << 1);
			tile[1][y] = byte1.TestPos(6) | (byte2.TestPos(6) << 1);
			tile[2][y] = byte1.TestPos(5) | (byte2.TestPos(5) << 1);
			tile[3][y] = byte1.TestPos(4) | (byte2.TestPos(4) << 1);
			tile[4][y] = byte1.TestPos(3) | (byte2.TestPos(3) << 1);
			tile[5][y] = byte1.TestPos(2) | (byte2.TestPos(2) << 1);
			tile[6][y] = byte1.TestPos(1) | (byte2.TestPos(1) << 1);
			tile[7][y] = byte1.TestPos(0) | (byte2.TestPos(0) << 1);

			++byte1Address;
			++byte2Address;
		}
	};

	static auto DrawTile = [] (Renderer& renderer, int x, int y, uint8 tile[8][8])
	{
		Color4 color;

		for (uint16 ty = 0; ty < 8; ++ty)
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
	const uint16 currNameTableAddress = PpuControl1::GetNameTableAddress(m_ppuControlReg1->Value());

	m_renderer->Clear();

	uint8 tile[8][8] = {0};
	for (uint16 y = 0; y < 30; ++y)
	{
		for (uint16 x = 0; x < 32; ++x)
		{
			const uint16 tileIndexAddress = currNameTableAddress + y*32 + x;
			const uint8 tileIndex = m_ppuMemoryBus->Read(tileIndexAddress);
			ReadTile(currBgPatternTableAddress, tileIndex, tile);
			DrawTile(*m_renderer, x*8, y*8, tile);
		}
	}

	m_renderer->Render();
}
