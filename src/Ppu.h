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
	void Execute(bool& finishedRender, uint32& numCpuCyclesToExecute);

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
	void Render();

	PpuMemoryBus* m_ppuMemoryBus;
	Nes* m_nes; //@TODO: Get rid of this dependency
	std::shared_ptr<Renderer> m_renderer;

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

	bool m_vramAndScrollFirstWrite; // $2005/2006 flip-flop

	uint16 m_vramAddress; // (V) - in PPU address space
	uint8 m_vramBufferedValue;

	struct ScrollData
	{
		uint8 fineOffsetX;
		uint8 fineOffsetY;
		uint8 coarseOffsetX;
		uint8 coarseOffsetY;
	};
	ScrollData m_scroll;
};
