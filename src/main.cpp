#include "Base.h"
#include "FileStream.h"
#include "Rom.h"
#include "Cpu.h"
#include "CpuRam.h"
#include <memory>

void PrintAppInfo()
{
	const char* text =
		"; nes-emu - Nintendo Entertainment System Emulator\n"
		"; Author: Antonio Maiorano (amaiorano at gmail dot com)\n"
		"; Source code available at http://github.com/amaiorano/nes-emu/\n"
		"\n";

	printf(text);
}

void PrintRomInfo(const char* inputFile, RomHeader& header)
{
	printf("; Input file: %s\n", inputFile);
	printf("; PRG ROM size: %d bytes\n", header.GetPrgRomSizeBytes());
	printf("; CHR ROM size: %d bytes\n", header.GetChrRomSizeBytes());
	printf("; Mapper number: %d\n", header.GetMapperNumber());
	printf("; Has SRAM: %s\n", header.HasSRAM()? "yes" : "no");
	printf("\n");
}

int ShowUsage(const char* appPath)
{
	printf("Usage: %s <nes rom>\n\n", appPath);
	return -1;
}

int main(int argc, char* argv[])
{
	try
	{
		PrintAppInfo();

		if (argc != 2)
			throw std::exception("Missing argument(s)");

		const char* inputFile = argv[1];

		FileStream fs(inputFile, "rb");

		RomHeader header;
		fs.Read((uint8*)&header, sizeof(RomHeader));

		if ( !header.IsValidHeader() )
			throw std::exception("Invalid header");

		// Next is Trainer, if present (0 or 512 bytes)
		if ( header.HasTrainer() )
			throw std::exception("Not supporting trainer roms");

		if ( header.IsPlayChoice10() || header.IsVSUnisystem() )
			throw std::exception("Not supporting arcade roms (Playchoice10 / VS Unisystem)");

		PrintRomInfo(inputFile, header);

		CpuRam cpuRam;

		// Next is PRG-ROM data (16384 * x bytes)
		const size_t prgRomSize = header.GetPrgRomSizeBytes();
		{
			uint8 prgRom[CpuRam::kPrgRomMaxSize] = {0};
			fs.Read(prgRom, prgRomSize);
			cpuRam.LoadPrgRom(prgRom, prgRomSize);
		}

		Cpu cpu;
		cpu.Initialize(cpuRam);
		cpu.Reset();
		cpu.Run();
	}
	catch (const std::exception& ex)
	{
		printf("%s\n", ex.what());
		return ShowUsage(argv[0]);
	}
	catch (...)
	{
		printf("Unknown exception\n");
		return ShowUsage(argv[0]);
	}

	return 0;
}
