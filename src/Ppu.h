#pragma once
#include "Base.h"
#include "Bitfield.h"
#include <memory>

class Renderer;
class Nes;
class CpuRam;
class PpuRam;
class SpriteRam;

class Ppu
{
public:
	Ppu();
	void Initialize(Nes& nes, CpuRam& cpuRam, PpuRam& ppuRam, SpriteRam& spriteRam);
	void Reset();
	void Run();

	void OnCpuMemoryPreRead(uint16 address);
	void OnCpuMemoryPostRead(uint16 address);
	void OnCpuMemoryPostWrite(uint16 address);

private:
	void Render();

	std::shared_ptr<Renderer> m_renderer;

	Nes* m_nes;
	CpuRam* m_cpuRam;
	PpuRam* m_ppuRam;
	SpriteRam* m_spriteRam;
	
	// Memory-mapped registers
	Bitfield8* m_ppuControlReg1;	// $2000
	Bitfield8* m_ppuControlReg2;	// $2001
	Bitfield8* m_ppuStatusReg;		// $2002
	uint8* m_sprRamAddressReg;		// $2003
	uint8* m_sprRamIoReg;			// $2004
	uint8* m_vramAddressReg1;		// $2005 bg pan/scroll	
	uint8* m_vramAddressReg2;		// $2006 sets high/low address to access via $2007
	uint8* m_vramIoReg;				// $2007	
	bool m_vramAddressReg2High;		// Indicates which byte of VRAM address will be set via $2006
	uint16 m_vramAddress;			// Current VRAM address
	uint8 m_vramBufferedValue;		// PPU quirk: first read from VRAM is invalid for $0000-3EFF
};
