#include "Cartridge.h"
#include "Nes.h"
#include "FileStream.h"
#include "Rom.h"
#include "MemoryMap.h"
#include "IO.h"
#include "Debugger.h"
#include "Mapper0.h"
#include "Mapper1.h"
#include "Mapper2.h"
#include "Mapper4.h"

namespace
{
	FORCEINLINE size_t GetBankIndex(uint16 address, uint16 baseAddress, size_t bankSize)
	{
		const size_t firstBankIndex = baseAddress / bankSize;
		return (address / bankSize) - firstBankIndex;
	}

	FORCEINLINE uint16 GetBankOffset(uint16 address, size_t bankSize)
	{
		return address & (bankSize - 1);
	}
}

void Cartridge::Initialize(Nes& nes)
{
	m_nes = &nes;
	m_mapper = nullptr;
}

RomHeader Cartridge::LoadRom(const char* file)
{
	m_romDirectory = IO::Path::GetDirectoryName(file);
	m_romFileNameNoExt = IO::Path::GetFileNameWithoutExtension(file);
	m_saveRamPath = IO::Path::Combine(m_romDirectory, m_romFileNameNoExt + ".sav");

	FileStream fs(file, "rb");
	RomHeader romHeader;

	fs.Read((uint8*)&romHeader, sizeof(RomHeader));

	if ( !romHeader.IsValidHeader() )
		FAIL("Invalid romHeader");

	// Next is Trainer, if present (0 or 512 bytes)
	if ( romHeader.HasTrainer() )
		FAIL("Not supporting trainer roms");

	if ( romHeader.IsPlayChoice10() || romHeader.IsVSUnisystem() )
		FAIL("Not supporting arcade roms (Playchoice10 / VS Unisystem)");

	// Zero out memory banks to ease debugging (not required)
	std::for_each(begin(m_prgBanks), end(m_prgBanks), [] (PrgBankMemory& m) { m.Initialize(); });
	std::for_each(begin(m_chrBanks), end(m_chrBanks), [] (ChrBankMemory& m) { m.Initialize(); });
	std::for_each(begin(m_savBanks), end(m_savBanks), [] (SavBankMemory& m) { m.Initialize(); });

	// PRG-ROM
	const size_t prgRomSize = romHeader.GetPrgRomSizeBytes();
	assert(prgRomSize % kPrgBankSize == 0);
	const size_t numPrgBanks = prgRomSize / kPrgBankSize;
	for (size_t i = 0; i < numPrgBanks; ++i)
	{
		fs.Read(m_prgBanks[i].RawPtr(), kPrgBankSize);
	}

	// CHR-ROM data
	const size_t chrRomSize = romHeader.GetChrRomSizeBytes();
	assert(chrRomSize % kChrBankSize == 0);
	const size_t numChrBanks = chrRomSize / kChrBankSize;

	if (chrRomSize > 0)
	{
		for (size_t i = 0; i < numChrBanks; ++i)
		{
			fs.Read(m_chrBanks[i].RawPtr(), kChrBankSize);
		}
	}

	size_t numSavBanks = romHeader.HasSRAM()? 1 : 0; // @TODO: Some boards switch sram banks (SOROM)

	switch (romHeader.GetMapperNumber())
	{
	case 0: m_mapperHolder.reset(new Mapper0()); break;
	case 1: m_mapperHolder.reset(new Mapper1()); break;
	case 2: m_mapperHolder.reset(new Mapper2()); break;
	case 4: m_mapperHolder.reset(new Mapper4()); break;
	default:
		FAIL("Unsupported mapper: %d", romHeader.GetMapperNumber());
	}
	m_mapper = m_mapperHolder.get();

	m_mapper->Initialize(numPrgBanks, numChrBanks, numSavBanks);

	m_cartNameTableMirroring = romHeader.GetNameTableMirroring();

	LoadSaveRamFile();

	return romHeader;
}

NameTableMirroring Cartridge::GetNameTableMirroring() const
{
	// Some mappers control mirroring, otherwise it's hard-wired on the cart
	auto result = m_mapper->GetNameTableMirroring();
	if (result != NameTableMirroring::Undefined)
	{
		return result;
	}
	return m_cartNameTableMirroring;
}

uint8 Cartridge::HandleCpuRead(uint16 cpuAddress)
{
	if (cpuAddress >= CpuMemory::kPrgRomBase)
	{
		return AccessPrgMem(cpuAddress);
	}
	else if (cpuAddress >= CpuMemory::kSaveRamBase)
	{
		// We don't bother with SRAM chip disable
		return AccessSavMem(cpuAddress);
	}
	
#if CONFIG_DEBUG
	if (!Debugger::IsExecuting())
		printf("Unhandled by mapper - read: $%04X\n", cpuAddress);
#endif

	return 0;
}

void Cartridge::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	m_mapper->OnCpuWrite(cpuAddress, value);

	if (cpuAddress >= CpuMemory::kPrgRomBase)
	{
		if (m_mapper->CanWritePrgMemory())
		{
			AccessPrgMem(cpuAddress) = value;
		}
	}
	else if (cpuAddress >= CpuMemory::kSaveRamBase)
	{
		if (m_mapper->CanWriteSavMemory())
		{
			AccessSavMem(cpuAddress) = value;
		}
	}
	else
	{
#if CONFIG_DEBUG
		if (!Debugger::IsExecuting())
			printf("Unhandled by mapper - write: $%04X\n", cpuAddress);
#endif
	}
}

uint8 Cartridge::HandlePpuRead(uint16 ppuAddress)
{
	return AccessChrMem(ppuAddress);
}

void Cartridge::HandlePpuWrite(uint16 ppuAddress, uint8 value)
{
	if (m_mapper->CanWriteChrMemory())
	{
		AccessChrMem(ppuAddress) = value;
	}
}

void Cartridge::WriteSaveRamFile()
{
	if (!IsRomLoaded())
		return;

	const size_t numSavBanks = m_mapper->NumSavBanks8k();
	if (numSavBanks == 0)
		return;

	FileStream saveFS;
	if (saveFS.Open(m_saveRamPath.c_str(), "wb"))
	{
		for (size_t i = 0; i < numSavBanks; ++i)
		{
			auto& bank = m_savBanks[i];
			saveFS.Write(bank.RawPtr(), kSavBankSize);
		}
		saveFS.Close();

		printf("Saved save ram file: %s\n", m_saveRamPath.c_str());
	}
}

void Cartridge::HACK_OnScanline()
{
	if (auto* mapper4 = dynamic_cast<Mapper4*>(m_mapper))
	{
		mapper4->HACK_OnScanline();
		if (mapper4->TestAndClearIrqPending())
		{
			m_nes->SignalCpuIrq();
		}
	}
}

void Cartridge::LoadSaveRamFile()
{
	const size_t numSavBanks = m_mapper->NumSavBanks8k();

	if (numSavBanks == 0)
		return;

	FileStream saveFS;
	if (saveFS.Open(m_saveRamPath.c_str(), "rb"))
	{
		for (size_t i = 0; i < m_mapper->NumSavBanks8k(); ++i)
		{
			auto& bank = m_savBanks[i];
			saveFS.Read(bank.RawPtr(), kSavBankSize);
		}
		saveFS.Close();

		printf("Loaded save ram file: %s\n", m_saveRamPath.c_str());
	}
}

uint8& Cartridge::AccessPrgMem(uint16 cpuAddress)
{
	const size_t bankIndex = GetBankIndex(cpuAddress, CpuMemory::kPrgRomBase, kPrgBankSize);
	const auto offset = GetBankOffset(cpuAddress, kPrgBankSize);
	const size_t mappedBankIndex = m_mapper->GetMappedPrgBankIndex(bankIndex);
	return m_prgBanks[mappedBankIndex].RawRef(offset);
}

uint8& Cartridge::AccessChrMem(uint16 ppuAddress)
{
	const size_t bankIndex = GetBankIndex(ppuAddress, PpuMemory::kChrRomBase, kChrBankSize);
	const uint16 offset = GetBankOffset(ppuAddress, kChrBankSize);
	const size_t mappedBankIndex = m_mapper->GetMappedChrBankIndex(bankIndex);
	return m_chrBanks[mappedBankIndex].RawRef(offset);
}

uint8& Cartridge::AccessSavMem(uint16 cpuAddress)
{
	const size_t bankIndex = GetBankIndex(cpuAddress, CpuMemory::kSaveRamBase, kSavBankSize);
	const uint16 offset = GetBankOffset(cpuAddress, kSavBankSize);
	const size_t mappedBankIndex = m_mapper->GetMappedSavBankIndex(bankIndex);
	return m_savBanks[mappedBankIndex].RawRef(offset);
}
