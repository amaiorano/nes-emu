#include "Cartridge.h"
#include "Nes.h"
#include "Stream.h"
#include "Rom.h"
#include "MemoryMap.h"
#include "Debugger.h"
#include "Mapper0.h"
#include "Mapper1.h"
#include "Mapper2.h"
#include "Mapper3.h"
#include "Mapper4.h"
#include "Mapper7.h"

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

void Cartridge::Serialize(class Serializer& serializer)
{
	SERIALIZE(m_cartNameTableMirroring);
	
	if (m_mapper->CanWritePrgMemory())
		SERIALIZE_BUFFER(m_prgBanks.data(), m_mapper->PrgMemorySize());

	if (m_mapper->CanWriteChrMemory())
		SERIALIZE_BUFFER(m_chrBanks.data(), m_mapper->ChrMemorySize());

	if (m_mapper->SavMemorySize() > 0)
		SERIALIZE_BUFFER(m_savBanks.data(), m_mapper->SavMemorySize());
	
	serializer.SerializeObject(*m_mapper);
}

RomHeader Cartridge::LoadRom(const char* file)
{
	FileStream fs(file, "rb");

	uint8 headerBytes[16];
	fs.ReadValue(headerBytes);
	RomHeader romHeader;
	romHeader.Initialize(headerBytes);

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

	// Note that "save" here doesn't imply battery-backed
	const size_t numSavBanks = romHeader.GetNumPrgRamBanks();
	assert(numSavBanks <= kMaxSavBanks);

	switch (romHeader.GetMapperNumber())
	{
	case 0: m_mapperHolder.reset(new Mapper0()); break;
	case 1: m_mapperHolder.reset(new Mapper1()); break;
	case 2: m_mapperHolder.reset(new Mapper2()); break;
	case 3: m_mapperHolder.reset(new Mapper3()); break;
	case 4: m_mapperHolder.reset(new Mapper4()); break;
	case 7: m_mapperHolder.reset(new Mapper7()); break;
	default:
		FAIL("Unsupported mapper: %d", romHeader.GetMapperNumber());
	}
	m_mapper = m_mapperHolder.get();

	m_mapper->Initialize(numPrgBanks, numChrBanks, numSavBanks);

	m_cartNameTableMirroring = romHeader.GetNameTableMirroring();
	m_hasSRAM = romHeader.HasSRAM();

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

void Cartridge::WriteSaveRamFile(const char* file)
{
	assert(IsRomLoaded());

	if (!m_hasSRAM)
		return;

	//@NOTE: We assume all prg-ram is battery-backed here, even though this may not
	// be true.

	const size_t numSavBanks = m_mapper->NumSavBanks8k();
	if (numSavBanks == 0)
		return;

	FileStream saveFS;
	if (saveFS.Open(file, "wb"))
	{
		for (size_t i = 0; i < numSavBanks; ++i)
		{
			auto& bank = m_savBanks[i];
			saveFS.Write(bank.RawPtr(), kSavBankSize);
		}
		saveFS.Close();

		printf("Saved save ram file: %s\n", file);
	}
}

void Cartridge::LoadSaveRamFile(const char* file)
{
	if (!m_hasSRAM)
		return;

	const size_t numSavBanks = m_mapper->NumSavBanks8k();
	if (numSavBanks == 0)
		return;

	FileStream saveFS;
	if (saveFS.Open(file, "rb"))
	{
		for (size_t i = 0; i < m_mapper->NumSavBanks8k(); ++i)
		{
			auto& bank = m_savBanks[i];
			saveFS.Read(bank.RawPtr(), kSavBankSize);
		}
		saveFS.Close();

		printf("Loaded save ram file: %s\n", file);
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

size_t Cartridge::GetPrgBankIndex16k(uint16 cpuAddress) const
{
	const size_t bankIndex4k = GetBankIndex(cpuAddress, CpuMemory::kPrgRomBase, kPrgBankSize);
	const size_t mappedBankIndex4k = m_mapper->GetMappedPrgBankIndex(bankIndex4k);
	return mappedBankIndex4k * KB(4) / KB(16);
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
