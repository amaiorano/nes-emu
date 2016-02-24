#include "Base.h"
#include "Nes.h"
#include "System.h"
#include "Input.h"
#include "Renderer.h"
#include "Debugger.h"

#include <vector>

#define kVersionMajor 1
#define kVersionMinor 3
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
			"### %s %s - Nintendo Entertainment System Emulator\n"
			"### Author: Antonio Maiorano (amaiorano at gmail dot com)\n"
			"### Source code available at http://github.com/amaiorano/nes-emu/ \n"
			"\n";

		printf(text, APP_NAME, kVersionString);
	}

	inline size_t BytesToKB(size_t bytes) { return bytes / 1024; }

	void PrintRomInfo(const char* romFile, const RomHeader& header)
	{
		printf("Rom Info:\n");
		printf("  File: %s\n", romFile);
		printf("  PRG ROM size: %d kb\n", BytesToKB(header.GetPrgRomSizeBytes()));
		printf("  CHR ROM size: %d kb\n", BytesToKB(header.GetChrRomSizeBytes()));
		printf("  PRG RAM size: %d kb\n", BytesToKB(header.GetPrgRamSizeBytes()));
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
		return System::OpenFileDialog(fileSelected, "Open NES rom", FILE_FILTER("NES Rom", "*.nes"));
	}

	void ProcessInputForChannelVolumes(Nes& nes)
	{
		struct ChannelState
		{
			ApuChannel::Type channel;
			SDL_Scancode key;
			bool enabled;
		};
		
		static ChannelState apuChannelState[] =
		{
			{ ApuChannel::Pulse1, SDL_SCANCODE_F1, true }, // We assume all channels are on (1.0f) by default
			{ ApuChannel::Pulse2, SDL_SCANCODE_F2, true },
			{ ApuChannel::Triangle, SDL_SCANCODE_F3, true },
			{ ApuChannel::Noise, SDL_SCANCODE_F4, true },
		};
		static_assert(ARRAYSIZE(apuChannelState) == ApuChannel::NumTypes, "Invalid size");

		for (auto& state : apuChannelState)
		{
			if (Input::KeyPressed(state.key))
			{
				state.enabled = !state.enabled;
				nes.SetChannelVolume(state.channel, state.enabled ? 1.0f : 0.0f);
			}
		}
	}
}

int main(int raw_argc, char* raw_argv[])
{
	//HACK: filter out Cocoa Application's specific arguments
	std::vector<char*> arg_list;
	for (int i = 0; i < raw_argc; ++i) {
		auto arg = raw_argv[i];
		if (!((arg[0] == '-' && arg[1] != '-' && strlen(arg) > 2) || strcmp(arg, "YES") == 0 || strcmp(arg, "NO") == 0))
		{
			arg_list.push_back(arg);
		}
	}
	
	int argc = (int)arg_list.size();
	char** argv = arg_list.data();
	
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

		std::shared_ptr<Nes> nesHolder = std::make_shared<Nes>();
		Nes* nes = nesHolder.get();
		nes->Initialize();
		
		NesDebugger::Initialize(*nes);

		RomHeader romHeader = nes->LoadRom(romFile.c_str());
		PrintRomInfo(romFile.c_str(), romHeader);
		nes->Reset();

		bool quit = false;
		bool paused = false;
		bool stepOneFrame = false;

		while (!quit)
		{
			Input::Update();
			
			NesDebugger::Update();

			nes->ExecuteFrame(paused);

			Renderer::SetWindowTitle( FormattedString<>("%s %s [FPS: %2.2f] %s", APP_NAME, kVersionString, nes->GetFps(), paused? "*PAUSED*" : "").Value() );

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

			// Restore pause state after stepping
			if (stepOneFrame)
			{
				stepOneFrame = false;
				paused = true;
			}

			if (Input::KeyPressed(SDL_SCANCODE_LEFTBRACKET) || Input::KeyDown(SDL_SCANCODE_RIGHTBRACKET))
			{
				stepOneFrame = true;
				paused = false; // Unpause for one frame
			}

			const bool turbo = Input::KeyDown(SDL_SCANCODE_GRAVE); // tilde '~' key
			nes->SetTurboEnabled(turbo);

			if (Input::KeyPressed(SDL_SCANCODE_F5))
			{
				nes->SerializeSaveState(true);
			}
			if (Input::KeyPressed(SDL_SCANCODE_F7))
			{
				nes->SerializeSaveState(false);
			}

			nes->RewindSaveStates(Input::KeyDown(SDL_SCANCODE_BACKSPACE));

			ProcessInputForChannelVolumes(*nes);
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

	NesDebugger::Shutdown();

	return 0;
}
