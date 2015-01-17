#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "Debugger.h"
#include "System.h"
#include "FrameTimer.h"
#include "Renderer.h"
#include "Input.h"

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
	return romHeader;
}

void Nes::Reset()
{
	m_cpu.Reset();
	m_ppu.Reset();
}

void Nes::Run()
{
	const float32 minFrameTime = 1.0f/60.0f;
	FrameTimer frameTimer;

	bool quit = false;

	while (!quit)
	{
		Input::Update();

		ExecuteCpuAndPpuFrame();

		// PPU just rendered a screen; FrameTimer will wait until we hit 60 FPS (if machine is too fast)
		frameTimer.Update(minFrameTime);
		Renderer::SetWindowTitle( FormattedString<>("nes-emu [FPS: %2.2f]", frameTimer.GetFps()).Value() );

		if (Input::KeyPressed(SDL_SCANCODE_F9))
		{
			m_cartridge.WriteSaveRamFile();
		}

		if (Input::AltDown() && Input::KeyPressed(SDL_SCANCODE_F4))
		{
			quit = true;
		}
	}
}

void Nes::ExecuteCpuAndPpuFrame()
{
	bool finishedRender = false;

	while (!finishedRender)
	{
		// Update CPU, get number of cycles elapsed
		uint32 cpuCycles;
		m_cpu.Execute(cpuCycles);

		// Update PPU with that many cycles
		const uint32 ppuCycles = cpuCycles * 3;
		m_ppu.Execute(ppuCycles, finishedRender);
	}
}
