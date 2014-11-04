#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "Debugger.h"

void Nes::Initialize()
{
	Debugger::Initialize(*this);
}

void Nes::LoadRom(const char* file)
{
	FileStream fs(file, "rb");

	fs.Read((uint8*)&m_romHeader, sizeof(RomHeader));

	if ( !m_romHeader.IsValidHeader() )
		throw std::exception("Invalid m_romHeader");

	// Next is Trainer, if present (0 or 512 bytes)
	if ( m_romHeader.HasTrainer() )
		throw std::exception("Not supporting trainer roms");

	if ( m_romHeader.IsPlayChoice10() || m_romHeader.IsVSUnisystem() )
		throw std::exception("Not supporting arcade roms (Playchoice10 / VS Unisystem)");

	// Next is PRG-ROM data (16K or 32K)
	const size_t prgRomSize = m_romHeader.GetPrgRomSizeBytes();
	{
		uint8 prgRom[CpuRam::kPrgRomMaxSize] = {0};
		fs.Read(prgRom, prgRomSize);
		m_cpuRam.LoadPrgRom(prgRom, prgRomSize);
	}

	// Next is CHR-ROM data (8K)
	const size_t chrRomSize = m_romHeader.GetChrRomSizeBytes();
	{
		uint8 chrRom[PpuRam::kChrRomSize];
		fs.Read(chrRom, chrRomSize);
		m_ppuRam.LoadChrRom(chrRom, chrRomSize);
	}

	m_cpu.Initialize(*this, m_cpuRam);
	m_ppu.Initialize(*this, m_cpuRam, m_ppuRam, m_spriteRam);
}

void Nes::Reset()
{
	m_cpu.Reset();
	m_ppu.Reset();
}

void Nes::Run()
{
	for (;;)
	{
		//@TODO: For now, run a bunch of CPU instructions for every PPU update. Eventually need to make it cycle based.
		for (size_t i = 0; i < 1; ++i)
		{
			m_cpu.Run();
		}
		m_ppu.Run();
	}
}
