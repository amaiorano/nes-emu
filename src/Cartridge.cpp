#include "Cartridge.h"
#include "FileStream.h"
#include "Rom.h"
#include "MemoryMap.h"
#include "MapperImpls.h"
#include "Debugger.h"

void Cartridge::Initialize()
{
}

RomHeader Cartridge::LoadRom(const char* file)
{
	FileStream fs(file, "rb");
	RomHeader romHeader;

	fs.Read((uint8*)&romHeader, sizeof(RomHeader));

	if ( !romHeader.IsValidHeader() )
		throw std::exception("Invalid romHeader");

	// Next is Trainer, if present (0 or 512 bytes)
	if ( romHeader.HasTrainer() )
		throw std::exception("Not supporting trainer roms");

	if ( romHeader.IsPlayChoice10() || romHeader.IsVSUnisystem() )
		throw std::exception("Not supporting arcade roms (Playchoice10 / VS Unisystem)");

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
	else // No CHR-ROM, zero out CHR-RAM
	{
		for (auto& bank : m_chrBanks)
		{
			bank.Initialize();
		}
	}

	switch (romHeader.GetMapperNumber())
	{
	case 0: m_mapperHolder.reset(new Mapper0()); break;
	case 2: m_mapperHolder.reset(new Mapper2()); break;
	default:
		throw std::exception(FormattedString<>("Unsupported mapper: %d", romHeader.GetMapperNumber()));
	}
	m_mapper = m_mapperHolder.get();
	
	m_mapper->Initialize(numPrgBanks, numChrBanks);

	m_screenArrangement = romHeader.GetScreenArrangement();

	return romHeader;
}

uint8 Cartridge::HandleCpuRead(uint16 cpuAddress)
{
	if (cpuAddress >= CpuMemory::kPrgRomBase)
	{
		return AccessPrgMem(cpuAddress);
	}
	else if (cpuAddress >= CpuMemory::kSaveRamBase)
	{
		return m_sram.Read(MapCpuToSram(cpuAddress));
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
		m_sram.Write(MapCpuToSram(cpuAddress), value);
	}
	else
	{
#if CONFIG_DEBUG
		if (!Debugger::IsExecuting())
			printf("Unhandled by mapper - write: $%04X\n", cpuAddress);
	}
#endif
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

uint8& Cartridge::AccessPrgMem(uint16 cpuAddress)
{
	const size_t cpuBankIndex = (cpuAddress / kPrgBankSize) - 8; // PRG-ROM/RAM starts at $8000, so subtract $8000/4K = 8 to get zero-based index
	const uint16 offset = cpuAddress & (kPrgBankSize - 1);
	const size_t mappedBankIndex = m_mapper->GetMappedPrgBankIndex(cpuBankIndex);
	return m_prgBanks[mappedBankIndex].RawRef(offset);
}

uint8& Cartridge::AccessChrMem(uint16 ppuAddress)
{
	const size_t ppuBankIndex = (ppuAddress / kChrBankSize);
	const uint16 offset = ppuAddress & (kChrBankSize - 1);
	const size_t mappedBankIndex = m_mapper->GetMappedChrBankIndex(ppuBankIndex);
	return m_chrBanks[mappedBankIndex].RawRef(offset);
}

uint16 Cartridge::MapCpuToSram(uint16 cpuAddress)
{
	assert(cpuAddress >= CpuMemory::kSaveRamBase && cpuAddress < CpuMemory::kSaveRamEnd);
	return cpuAddress - CpuMemory::kSaveRamBase;
}
