#include "Base.h"
#include "Nes.h"
#include "System.h"
#include "Input.h"

#define kVersionMajor 1
#define kVersionMinor 0
#if CONFIG_DEBUG
	#define kVersionConfig "d"
#else
	#define kVersionConfig ""
#endif
const char* kVersionString = "v" STRINGIZE(kVersionMajor) "." STRINGIZE(kVersionMinor) kVersionConfig;

namespace
{
	void PrintAppInfo()
	{
		const char* text =
			"### nes-emu %s - Nintendo Entertainment System Emulator\n"
			"### Author: Antonio Maiorano (amaiorano at gmail dot com)\n"
			"### Source code available at http://github.com/amaiorano/nes-emu/ \n"
			"\n";

		printf(text, kVersionString);
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

	bool OpenRomFileDialog(std::string& fileSelected)
	{
		return System::OpenFileDialog(fileSelected, "Open NES rom", "NES rom *.nes\0*.nes\0");
	}
}

int main(int argc, char* argv[])
{
	try
	{
		PrintAppInfo();

		std::string romFile;

		if (argc == 1)
		{
			std::string fileSelected;
			if (OpenRomFileDialog(fileSelected))
			{
				romFile = fileSelected;
			}
		}
		else if (argc == 2)
		{
			romFile = argv[1];
		}
		
		if (romFile.empty())
		{
			ShowUsage(argv[0]);
			FAIL("No rom file to load");
		}

		std::shared_ptr<Nes> nes = std::make_shared<Nes>();
		nes->Initialize();

		RomHeader romHeader = nes->LoadRom(romFile.c_str());
		PrintRomInfo(romFile.c_str(), romHeader);
		nes->Reset();

		bool quit = false;
		bool paused = false;
		bool stepOneFrame = false;
		bool turbo = false;

		while (!quit)
		{
			Input::Update();

			nes->ExecuteFrame(paused && !stepOneFrame, turbo);

			if (Input::CtrlDown() && Input::KeyPressed(SDL_SCANCODE_O))
			{
				std::string fileSelected;
				if (OpenRomFileDialog(fileSelected))
				{
					romFile = fileSelected;
					romHeader = nes->LoadRom(romFile.c_str());
					PrintRomInfo(romFile.c_str(), romHeader);
					nes->Reset();
				}
			}

			if (Input::CtrlDown() && Input::KeyPressed(SDL_SCANCODE_R))
			{
				nes->Reset();
				paused = false;
			}

			if (Input::AltDown() && Input::KeyPressed(SDL_SCANCODE_F4))
			{
				quit = true;
			}

			if (Input::KeyPressed(SDL_SCANCODE_P))
			{
				paused = !paused;
			}

			stepOneFrame = false;
			if (Input::KeyPressed(SDL_SCANCODE_LEFTBRACKET) || Input::KeyDown(SDL_SCANCODE_RIGHTBRACKET))
			{
				paused = true;
				stepOneFrame = true;
			}

			turbo = Input::KeyDown(SDL_SCANCODE_GRAVE); // tilde '~' key
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
