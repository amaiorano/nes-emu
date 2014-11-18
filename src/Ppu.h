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

	void Render();

	PpuMemoryBus* m_ppuMemoryBus;
	Nes* m_nes; //@TODO: Get rid of this dependency
	std::shared_ptr<Renderer> m_renderer;

	// Memory used to store name/attribute tables (aka CIRAM)
	typedef Memory<FixedSizeStorage<KB(2)>> NameTableMemory;
	NameTableMemory m_nameTables;

	typedef Memory<FixedSizeStorage<32>> PaletteMemory;
	PaletteMemory m_palette;
	
	// Memory mapped registers
	typedef Memory<FixedSizeStorage<8>> PpuRegisterMemory; // $2000 - $2007
	PpuRegisterMemory m_ppuRegisters;

	// Internal registers for handling the "vram pointer" controlled via $2006/$2007 in CPU address space.
	bool m_vramAddressHigh; // $2006 latch (T)
	uint16 m_vramAddress; // (V) - in PPU address space
	uint8 m_vramBufferedValue;

	typedef Memory<FixedSizeStorage<256>> SpriteMemory; // (aka OAM or SPR-RAM)
	SpriteMemory m_sprites;

	// Memory-mapped registers
	Bitfield8* m_ppuControlReg1;	// $2000
	Bitfield8* m_ppuControlReg2;	// $2001
	Bitfield8* m_ppuStatusReg;		// $2002
};
