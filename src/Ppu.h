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

	void OnCpuMemoryRead(uint16 address);
	void OnCpuMemoryWrite(uint16 address);

private:
	void Render();

	std::shared_ptr<Renderer> m_renderer;

	Nes* m_nes;
	CpuRam* m_cpuRam;
	PpuRam* m_ppuRam;
	SpriteRam* m_spriteRam;
	
	Bitfield8* m_ppuControl1;
	Bitfield8* m_ppuControl2;
	Bitfield8* m_ppuStatus;
	
	// Data for memory-mapped registers at $2006-$2007 in CPU RAM
	struct VRamAddress
	{
		VRamAddress() : high(true), address(0) {}
		bool high;
		uint16 address;
	} m_vramAddress;
};
