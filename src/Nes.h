#pragma once

#include "Cpu.h"
#include "Ppu.h"
#include "Apu.h"
#include "Cartridge.h"
#include "Memory.h"
#include "CpuInternalRam.h"
#include "MemoryBus.h"
#include "FrameTimer.h"
#include "RewindManager.h"

class Nes
{
public:
	~Nes();

	void Initialize();
	
	RomHeader LoadRom(const char* file);
	void Reset();

	bool SerializeSaveState(bool save);
	void Serialize(class Serializer& serializer);

	void RewindSaveStates(bool enable);

	void ExecuteFrame(bool paused);

	void SetTurboEnabled(bool enabled) { m_turbo = enabled; }
	void SetChannelVolume(ApuChannel::Type type, float32 volume) { m_apu.SetChannelVolume(type, volume); }

	void SignalCpuNmi() { m_cpu.Nmi(); }
	void SignalCpuIrq() { m_cpu.Irq(); }

	float64 GetFps() const { return m_frameTimer.GetFps(); }
	NameTableMirroring GetNameTableMirroring() const { return m_cartridge.GetNameTableMirroring(); }
	void HACK_OnScanline() { m_cartridge.HACK_OnScanline(); }

private:
	friend class DebuggerImpl;

	void ExecuteCpuAndPpuFrame();
	void SerializeSaveRam(bool save);

	Cpu m_cpu;
	Ppu m_ppu;
	Apu m_apu; // Maybe should just be aggregated and updated by Cpu?
	Cartridge m_cartridge;
	CpuInternalRam m_cpuInternalRam;
	CpuMemoryBus m_cpuMemoryBus;
	PpuMemoryBus m_ppuMemoryBus;

	FrameTimer m_frameTimer;
	RewindManager m_rewindManager;

	std::string m_romName;
	std::string m_saveDir;

	float64 m_lastSaveRamTime;
	bool m_turbo;
};
