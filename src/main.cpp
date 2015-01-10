#include "Base.h"
#include "Nes.h"
#include "System.h"

namespace
{
	void PrintAppInfo()
	{
		const char* text =
			"; nes-emu - Nintendo Entertainment System Emulator\n"
			"; Author: Antonio Maiorano (amaiorano at gmail dot com)\n"
			"; Source code available at http://github.com/amaiorano/nes-emu/"
			"\n\n";

		printf(text);
	}

	void PrintRomInfo(const char* inputFile, const RomHeader& header)
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
}

int main(int argc, char* argv[])
{
	try
	{
		PrintAppInfo();

		if (argc != 2)
		{
			ShowUsage(argv[0]);
			throw std::exception("Missing argument(s)");
		}

		const char* inputFile = argv[1];

		std::shared_ptr<Nes> nes = std::make_shared<Nes>();
		nes->Initialize();

		const RomHeader romHeader = nes->LoadRom(inputFile);
		PrintRomInfo(inputFile, romHeader);
		nes->Reset();
		nes->Run();
	}
	catch (const std::exception& ex)
	{
		System::MessageBox("Exception", ex.what());
	}
	catch (...)
	{
		System::MessageBox("Exception", "Unknown exception");
	}

	return 0;
}
