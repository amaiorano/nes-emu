#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "Debugger.h"
#include "System.h"
#include "FrameTimer.h"
#include "Renderer.h"

void Nes::Initialize()
{
	Debugger::Initialize(*this);

	m_cpu.Initialize(m_cpuMemoryBus);
	m_ppu.Initialize(m_ppuMemoryBus, *this);
	m_cartridge.Initialize();
	m_cpuInternalRam.Initialize();
	m_cpuMemoryBus.Initialize(m_cpu, m_ppu, m_cartridge, m_cpuInternalRam);
	m_ppuMemoryBus.Initialize(m_ppu, m_cartridge);
}

RomHeader Nes::LoadRom(const char* file)
{
	RomHeader romHeader = m_cartridge.LoadRom(file);

	switch (m_cartridge.GetScreenArrangement())
	{
	case ScreenArrangement::FourScreen:
		assert(false && "Four screen name tables not yet supported");
		// Fall through... same case as Horizontal (PPU VRAM is used for top 2 virtual screens, bottom two is on cart)

	case ScreenArrangement::Horizontal:
		m_ppu.SetNameTableVerticalMirroring(true);
		break;

	case ScreenArrangement::Vertical:
		m_ppu.SetNameTableVerticalMirroring(false);
		break;
	}

	return romHeader;
}

void Nes::Reset()
{
	m_cpu.Reset();
	m_ppu.Reset();
}

void Nes::Run()
{
	int32 numCpuCyclesLeft = 0;

	const float32 minFrameTime = 1.0f/60.0f;
	FrameTimer frameTimer;

	for (;;)
	{
		bool finishedRender = false;
		while (!finishedRender)
		{
			// PPU update
			uint32 numCpuCyclesToExecute = 0;
			m_ppu.Execute(finishedRender, numCpuCyclesToExecute);
			numCpuCyclesLeft += numCpuCyclesToExecute;
			assert(numCpuCyclesLeft > 0);

			// CPU update
			uint32 actualCpuCycles = 0;
			m_cpu.Execute(numCpuCyclesLeft, actualCpuCycles);
			numCpuCyclesLeft -= actualCpuCycles; // Usually negative
		}

		// PPU just rendered a screen; FrameTimer will wait until we hit 60 FPS (if machine is too fast)
		frameTimer.Update(minFrameTime);
		Renderer::SetWindowTitle( FormattedString<>("nes-emu [FPS: %2.2f]", frameTimer.GetFps()).Value() );
	}
}
