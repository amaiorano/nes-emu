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
	void Run();

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

private:
	uint16 MapCpuToPpuRegister(uint16 cpuAddress);
	uint16 MapPpuToVRam(uint16 ppuAddress);
	uint16 MapPpuToPalette(uint16 ppuAddress);

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
	typedef Memory<FixedSizeStorage<8>> PpuRegisterMemory;
	PpuRegisterMemory m_ppuRegisters;

	// Internal registers for handling the "vram pointer" controlled via $2006/$2007 in CPU memory space.
	// The "vram" is a bit of a misnomer, as it can be used to address all PPU memory.
	bool m_vramAddressHigh;
	uint16 m_vramAddress; // In PPU address space
	uint8 m_vramBufferedValue;

	// Memory-mapped registers
	Bitfield8* m_ppuControlReg1;	// $2000
	Bitfield8* m_ppuControlReg2;	// $2001
	Bitfield8* m_ppuStatusReg;		// $2002
};
