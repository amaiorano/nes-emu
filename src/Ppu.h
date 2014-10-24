#pragma once
#include "Base.h"

class CpuRam;
class PpuRam;
class SpriteRam;

class Ppu
{
public:
	Ppu();
	void Initialize(CpuRam& cpuRam, PpuRam& ppuRam, SpriteRam& spriteRam);
	void Reset();
	void Run();

private:
	CpuRam* m_cpuRam;
	PpuRam* m_ppuRam;
	SpriteRam* m_spriteRam;
};
