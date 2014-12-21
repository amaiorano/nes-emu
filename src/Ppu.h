#pragma once
#include "Base.h"
#include "Memory.h"
#include "Bitfield.h"
#include <memory>

class Renderer;
class PpuMemoryBus;
class Nes;

class Ppu
{
public:
	Ppu();
	void Initialize(PpuMemoryBus& ppuMemoryBus, Nes& nes);
	void SetNameTableVerticalMirroring(bool enabled) { m_nameTableVerticalMirroring = enabled; }

	void Reset();
	void Execute(uint32 ppuCycles, bool& finishedRender);

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

private:
	uint16 MapCpuToPpuRegister(uint16 cpuAddress);
	uint16 MapPpuToVRam(uint16 ppuAddress);
	uint16 MapPpuToPalette(uint16 ppuAddress);

	uint8 ReadPpuRegister(uint16 cpuAddress);
	void WritePpuRegister(uint16 cpuAddress, uint8 value);

	void ClearBackground();
	void FetchBackgroundTileData();
	void RenderPixel(uint32 x, uint32 y);

	PpuMemoryBus* m_ppuMemoryBus;
	Nes* m_nes; //@TODO: Get rid of this dependency
	std::shared_ptr<Renderer> m_rendererHolder;
	Renderer* m_renderer;

	// Hardware wise, setting this to true is like connecting PPU A10 to CIRAM A10, else PPU A11 to CIRAM A10.
	// This is done on cartridges by shorting the "V" or "H" solder pads.
	bool m_nameTableVerticalMirroring;

	// Memory used to store name/attribute tables (aka CIRAM)
	typedef Memory<FixedSizeStorage<KB(2)>> NameTableMemory;
	NameTableMemory m_nameTables;

	typedef Memory<FixedSizeStorage<32>> PaletteMemory;
	PaletteMemory m_palette;

	static const size_t kMaxSprites = 64;
	static const size_t kSpriteDataSize = 4;
	static const size_t kSpriteMemorySize = kMaxSprites * kSpriteDataSize;
	typedef Memory<FixedSizeStorage<kSpriteMemorySize>> SpriteMemory; // (aka OAM or SPR-RAM)
	SpriteMemory m_sprites;
	
	// Memory mapped registers
	typedef Memory<FixedSizeStorage<8>> PpuRegisterMemory; // $2000 - $2007
	PpuRegisterMemory m_ppuRegisters;

	// Memory-mapped registers
	Bitfield8* m_ppuControlReg1;	// $2000
	Bitfield8* m_ppuControlReg2;	// $2001
	Bitfield8* m_ppuStatusReg;		// $2002

	bool m_vramAndScrollFirstWrite;	// $2005/2006 flip-flop, "Loopy w"
	uint16 m_vramAddress;			// "Loopy v"
	uint16 m_tempVRamAddress;		// "Loopy t"
	uint8 m_fineX;					// Fine x scroll (3 bits), "Loopy x"
	uint8 m_vramBufferedValue;

	uint32 m_cycle;
	bool m_evenFrame;

	struct BgTileFetchData
	{
		uint8 lowBg;
		uint8 highBg;
		uint8 paletteHighBits;

#if CONFIG_DEBUG
		struct
		{
			uint16 vramAddress;
			uint16 tileIndexAddress;
			uint16 attributeAddress;
			uint16 attributeShift;
			uint16 byte1Address;
		} debug;
#endif
	};
	BgTileFetchData m_bgTileFetchDataPipeline[2];
};
