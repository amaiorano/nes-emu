#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "System.h"
#include "Renderer.h"

Nes::~Nes()
{
	// Save sram on exit
	m_cartridge.WriteSaveRamFile();
}

void Nes::Initialize()
{
	m_cpu.Initialize(m_cpuMemoryBus);
	m_ppu.Initialize(m_ppuMemoryBus, *this);
	m_cartridge.Initialize(*this);
	m_cpuInternalRam.Initialize();
	m_cpuMemoryBus.Initialize(m_cpu, m_ppu, m_cartridge, m_cpuInternalRam);
	m_ppuMemoryBus.Initialize(m_ppu, m_cartridge);
	m_turbo = false;
}

RomHeader Nes::LoadRom(const char* file)
{
	// Save sram of current cart before loading a new one
	m_cartridge.WriteSaveRamFile();

	RomHeader romHeader = m_cartridge.LoadRom(file);
	return romHeader;
}

void Nes::Reset()
{
	m_frameTimer.Reset();
	m_cpu.Reset();
	m_ppu.Reset();
	//@TODO: Maybe reset cartridge (and mapper)?

	m_lastSaveRamTime = System::GetTimeSec();
}

void Nes::ExecuteFrame(bool paused)
{
	if (!paused)
	{
		ExecuteCpuAndPpuFrame();
		m_ppu.RenderFrame();
	}

	// Just rendered a screen; FrameTimer will wait until we hit 60 FPS (if machine is too fast).
	// If turbo mode is enabled, it won't wait.
	const float32 minFrameTime = 1.0f/60.0f;
	m_frameTimer.Update(m_turbo? 0.f: minFrameTime);

	// Auto-save sram at fixed intervals
	const float64 saveInterval = 5.0;
	float64 currTime = System::GetTimeSec();
	if (currTime - m_lastSaveRamTime >= saveInterval)
	{
		m_cartridge.WriteSaveRamFile();
		m_lastSaveRamTime = currTime;
	}
}

void Nes::ExecuteCpuAndPpuFrame()
{
	bool completedFrame = false;

	while (!completedFrame)
	{
		// Update CPU, get number of cycles elapsed
		uint32 cpuCycles;
		m_cpu.Execute(cpuCycles);

		// Update PPU with that many cycles
		const uint32 ppuCycles = cpuCycles * 3;
		m_ppu.Execute(ppuCycles, completedFrame);
	}
}
