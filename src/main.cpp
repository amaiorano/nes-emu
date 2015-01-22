#include "Base.h"
#include "Nes.h"
#include "System.h"
#include "Input.h"

namespace
{
	void PrintAppInfo()
	{
		const char* text =
			"### nes-emu - Nintendo Entertainment System Emulator\n"
			"### Author: Antonio Maiorano (amaiorano at gmail dot com)\n"
			"### Source code available at http://github.com/amaiorano/nes-emu/ \n"
			"\n";

		printf(text);
	}

	inline size_t BytesToKB(size_t bytes) { return bytes / 1024; }

	void PrintRomInfo(const char* romFile, const RomHeader& header)
	{
		printf("Rom Info:\n");
		printf("  File: %s\n", romFile);
		printf("  PRG ROM size: %d kb\n", BytesToKB(header.GetPrgRomSizeBytes()));
		printf("  CHR ROM size: %d kb\n", BytesToKB(header.GetChrRomSizeBytes()));
		printf("  Mapper number: %d\n", header.GetMapperNumber());
		printf("  Has SRAM: %s\n", header.HasSRAM()? "yes" : "no");
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
			FAIL("Missing argument(s)");
		}

		const char* romFile = argv[1];

		std::shared_ptr<Nes> nes = std::make_shared<Nes>();
		nes->Initialize();

		{
			const RomHeader romHeader = nes->LoadRom(romFile);
			PrintRomInfo(romFile, romHeader);
			nes->Reset();
		}

		bool quit = false;
		while (!quit)
		{
			Input::Update();

			nes->ExecuteFrame();

			if (Input::CtrlDown() && Input::KeyPressed(SDL_SCANCODE_R))
			{
				nes->Reset();
			}

			if (Input::AltDown() && Input::KeyPressed(SDL_SCANCODE_F4))
			{
				quit = true;
			}
		}
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
