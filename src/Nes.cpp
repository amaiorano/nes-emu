#include "Nes.h"
#include "Stream.h"
#include "Rom.h"
#include "System.h"
#include "Serializer.h"
#include "Renderer.h"
#include "IO.h"
#include "CircularBuffer.h"

Nes::~Nes()
{
	// Save sram on exit
	SerializeSaveRam(true);
}

void Nes::Initialize()
{
	m_apu.Initialize();
	m_cpu.Initialize(m_cpuMemoryBus, m_apu);
	m_ppu.Initialize(m_ppuMemoryBus, *this);
	m_cartridge.Initialize(*this);
	m_cpuInternalRam.Initialize();
	m_cpuMemoryBus.Initialize(m_cpu, m_ppu, m_cartridge, m_cpuInternalRam);
	m_ppuMemoryBus.Initialize(m_ppu, m_cartridge);
	m_turbo = false;

	// Create directories
	const std::string& appDir = System::GetAppDirectory();
	m_saveDir = appDir + "saves/";
	System::CreateDirectory(m_saveDir.c_str());
}

RomHeader Nes::LoadRom(const char* file)
{
	// Save sram of current cart before loading a new one
	SerializeSaveRam(true);

	m_romName = IO::Path::GetFileNameWithoutExtension(file);

	// Load rom and last sram state, if any
	RomHeader romHeader = m_cartridge.LoadRom(file);
	SerializeSaveRam(false);

	// Initialize rewind buffer
	m_rewindManager.Initialize(*this);

	return romHeader;
}

void Nes::Reset()
{
	m_frameTimer.Reset();
	m_cpu.Reset();
	m_ppu.Reset();
	m_apu.Reset();
	//@TODO: Maybe reset cartridge (and mapper)?

	m_lastSaveRamTime = System::GetTimeSec();
}

void Nes::SerializeSaveRam(bool save)
{
	if (!m_cartridge.IsRomLoaded())
		return;

	assert(!m_romName.empty());
	const std::string& saveRamePath = m_saveDir + m_romName + ".sav";

	if (save)
	{
		m_cartridge.WriteSaveRamFile(saveRamePath.c_str());
	}
	else
	{
		m_cartridge.LoadSaveRamFile(saveRamePath.c_str());
	}
}

bool Nes::SerializeSaveState(bool save)
{
	const std::string& saveStatePath = m_saveDir + m_romName + ".st0";

	try
	{
		if (save)
		{
			FileStream fs;
			if (!fs.Open(saveStatePath.c_str(), "wb"))
				throw std::logic_error("Failed to open file for save");

			Serializer::SaveRootObject(fs, *this);
		}
		else
		{
			FileStream fs;
			if (!fs.Open(saveStatePath.c_str(), "rb"))
			{
				throw std::logic_error("Failed to open file for load");
			}

			Reset();
			Serializer::LoadRootObject(fs, *this);

			// Clear rewind states so user can't rewind to before this save state was loaded
			m_rewindManager.ClearRewindStates();
		}

		printf("%s SaveState: %s\n", save ? "Saved" : "Loaded", saveStatePath.c_str());
		return true;
	}
	catch (const std::exception& ex)
	{
		printf("Failed to %s SaveState: %s, Reason: %s\n", save ? "Save" : "Load", saveStatePath.c_str(), ex.what());
	}
	return false;
}

void Nes::Serialize(class Serializer& serializer)
{
	SERIALIZE(m_turbo);
	serializer.SerializeObject(m_cpu);
	serializer.SerializeObject(m_ppu);
	serializer.SerializeObject(m_apu);
	serializer.SerializeObject(m_cartridge);
	serializer.SerializeObject(m_cpuInternalRam);
}

void Nes::RewindSaveStates(bool enable)
{
	m_rewindManager.SetRewinding(enable);
}

void Nes::ExecuteFrame(bool paused)
{
	if (m_rewindManager.IsRewinding())
	{
		if (m_rewindManager.RewindFrame())
		{
			// Execute a single frame so that we can render it and play audio
			ExecuteCpuAndPpuFrame();
			m_ppu.RenderFrame();
		}
		return;
	}

	if (!paused)
	{
		ExecuteCpuAndPpuFrame();
		m_ppu.RenderFrame();

		m_rewindManager.SaveRewindState();
	}

	// Just rendered a screen; FrameTimer will wait until we hit 60 FPS (if machine is too fast).
	// If turbo mode is enabled, it won't wait.
	const float32 minFrameTime = 1.0f/60.0f;
	m_frameTimer.Update(m_turbo? 0.f: minFrameTime);

	// Auto-save sram at fixed intervals
	const float64 saveInterval = 5.0;
	const float64 currTime = System::GetTimeSec();
	if (currTime - m_lastSaveRamTime >= saveInterval)
	{
		SerializeSaveRam(true);
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
		m_ppu.Execute(cpuCycles, completedFrame);

		m_apu.Execute(cpuCycles);
	}
}
