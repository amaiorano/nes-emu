#include "Debugger.h"

#if DEBUGGING_ENABLED

#include "Base.h"
#include "Nes.h"
#include "OpCodeTable.h"
#include "System.h"
#include "FileStream.h"
#include <cstdio>
#include <cassert>
#include <stdexcept>

#define ADDR_8 "$%02X"
#define ADDR_16 "$%04X"

namespace
{
	bool g_logOpsEnabled = true;

	void LogOp(const char* format, ...)
	{
		if (g_logOpsEnabled)
		{
			va_list args;
			va_start(args, format);
			vprintf(format, args);
			va_end(args);
		}
	}
}

class DebuggerImpl
{
public:
	DebuggerImpl()
		: m_nes(nullptr)
	{
	}

	void Initialize(Nes& nes)
	{
		m_nes = &nes;
	}

	template <typename T>
	void MemoryDump(T& memory, FileStream& fs, uint16 start = 0x0000, size_t size = 0, size_t bytesPerLine = 16)
	{
		if (size == 0)
			size = memory.MemorySize();

		const uint32 from = start;
		const uint32 to = from + size - 1;
		const uint8* pmem = memory.UnsafePtr(0);
		size_t bytesPerLineCount = 0;

		for (uint32 curr = from; curr <= to; ++curr)
		{
			if (bytesPerLineCount == 0)
			{
				if (curr != from)
					fs.Printf("\n");
				fs.Printf(ADDR_16": ", curr);
			}
			fs.Printf("%02X ", pmem[curr]);
			bytesPerLineCount = (bytesPerLineCount + 1) % bytesPerLine;
		}		
		fs.Printf("\n");
	}

	template <typename T>
	void MemoryDump(T& memory, const char* file, uint16 start = 0x0000, size_t size = 0)
	{
		FileStream fs(file, "w");
		MemoryDump(memory, fs, start, size);
	}

	void MemoryDumpPpuRam(PpuRam& ppuRam)
	{
		// Dump full memory
		const char* file = "PpuRam.dmp";
		printf("Dumping: %s\n", file);
		MemoryDump(ppuRam, "PpuRam.dmp");
		
		// Dump detailed break down
		file = "PpuRam-detail.dmp";
		printf("Dumping: %s\n", file);

		FileStream fs(file, "w");

		for (size_t i = 0; i < PpuRam::kNumPatternTables; ++i)
		{
			fs.Printf("Pattern Table %d\n\n", i);
			MemoryDump(ppuRam, fs, PpuRam::GetPatternTableAddress(i), PpuRam::kPatternTableSize, 32*2);
		}

		for (size_t i = 0; i < PpuRam::kNumMaxNameTables; ++i)
		{
			fs.Printf("\nName Table %d\n\n", i);
			MemoryDump(ppuRam, fs, PpuRam::GetNameTableAddress(i), PpuRam::kNameTableSize, 32);

			fs.Printf("\nAttribute Table %d\n\n", i);
			MemoryDump(ppuRam, fs, PpuRam::GetAttributeTableAddress(i), PpuRam::kAttributeTableSize, 32);
		}

		fs.Printf("\nImage Palette\n\n");
		MemoryDump(ppuRam, fs, PpuRam::kImagePalette, PpuRam::kPaletteSize);

		fs.Printf("\nSprite Palette\n\n");
		MemoryDump(ppuRam, fs, PpuRam::kImagePalette, PpuRam::kPaletteSize);
	}

	void Update()
	{
		const char KEY_SPACE = 32;
		const char KEY_ENTER = 13;

		static bool stepMode = true;

		char key;
		if (stepMode)
		{
			bool done = false;
			bool showPrompt = true;
			
			while (!done)
			{
				if (showPrompt)
				{
					printf("> ");
					showPrompt = false;
				}

				key = (char)tolower(System::WaitForKeyPress());
				//printf("key pressed: %c (%d)\n", key, key);

				switch (key)
				{
				case KEY_SPACE:
				case KEY_ENTER:
					printf("\n");
					done = true;
					break;

				case 'q':
					throw std::exception("User quit from debugger");
					break;

				case 'g':
					stepMode = false;
					done = true;
					break;

				case 'l':
					g_logOpsEnabled = !g_logOpsEnabled;
					printf("Logging operations: %s\n", g_logOpsEnabled? "on" : "off");
					showPrompt = true;
					break;

				case 'd':
					{
						MemoryDumpPpuRam(m_nes->m_ppuRam);						
						
						printf("Dumping: CpuRam.dmp\n");
						MemoryDump(m_nes->m_cpuRam, "CpuRam.dmp");
						
						printf("Dumping: SpriteRam.dmp\n");
						MemoryDump(m_nes->m_spriteRam, "SpriteRam.dmp");
						
						showPrompt = true;
					}
					break;
				}
			}
		}
		else if (System::GetKeyPress(key))
		{
			stepMode = true;
		}
	}

	void PreCpuInstruction()
	{
		Cpu& cpu = m_nes->m_cpu;
		OpCodeEntry& opCodeEntry = *cpu.m_pEntry;
		CpuRam& cpuRam = m_nes->m_cpuRam;
		const uint16 PC = cpu.PC;

		// Print PC
		LogOp(ADDR_16 "\t", PC);

		// Print instruction in hex
		for (uint16 i = 0; i < 4; ++i)
		{
			if (i < opCodeEntry.numBytes)
				LogOp("%02X", cpuRam.Read8(PC + i));
			else
				LogOp(" ");
		}
		LogOp("\t");

		// Print opcode name
		LogOp("%s ", OpCodeName::String[opCodeEntry.opCodeName]);

		// Print operand
		switch (opCodeEntry.addrMode)
		{
		case AddressMode::Immedt:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp("#" ADDR_8, address);
			}
			break;

		case AddressMode::Implid:
			// No operand to output
			break;

		case AddressMode::Accumu:
			{
				LogOp("A");
			}
			break;

		case AddressMode::Relatv:
			{
				// For branch instructions, resolve the target address and print it in comments
				const int8 offset = cpuRam.Read8(PC+1); // Signed offset in [-128,127]
				const uint16 target = PC + opCodeEntry.numBytes + offset;
				LogOp(ADDR_8 " ; " ADDR_16 " (%d)", (uint8)offset, target, offset);
			}
			break;

		case AddressMode::ZeroPg:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp(ADDR_8, address);
			}
			break;

		case AddressMode::ZPIdxX:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp(ADDR_8 ",X", address);
			}
			break;

		case AddressMode::ZPIdxY:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp(ADDR_8 ",Y", address);
			}
			break;

		case AddressMode::Absolu:
			{
				uint16 address = cpuRam.Read16(PC+1);
				LogOp(ADDR_16, address);
			}
			break;

		case AddressMode::AbIdxX:
			{
				uint16 address = cpuRam.Read16(PC+1);
				LogOp(ADDR_16 ",X", address);
			}
			break;

		case AddressMode::AbIdxY:
			{
				uint16 address = cpuRam.Read16(PC+1);
				LogOp(ADDR_16 ",Y", address);
			}
			break;

		case AddressMode::Indrct:
			{
				uint16 address = cpuRam.Read16(PC+1);
				LogOp("(" ADDR_16 ")", address);
			}
			break;

		case AddressMode::IdxInd:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp("(" ADDR_8 ",X)", address);
			}
			break;

		case AddressMode::IndIdx:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				LogOp("(" ADDR_8 "),Y", address);
			}
			break;

		default:
			assert(false && "Invalid addressing mode");
			break;
		}

		LogOp("\n");
	}

	void PostCpuInstruction()
	{
		Cpu& cpu = m_nes->m_cpu;
		CpuRam& cpuRam = m_nes->m_cpuRam;

		static const char StatusFlagNames[] =
		{
			'C',
			'Z',
			'I',
			'D',
			'B',
			'U',
			'V',
			'N',
		};

		using namespace StatusFlag;

#define HILO(v) (cpu.P.Test(v) ? StatusFlagNames[BitFlagToPos<v>::Result-1] : tolower(StatusFlagNames[BitFlagToPos<v>::Result-1]))

		LogOp("  SP="ADDR_8" A="ADDR_8" X="ADDR_8" Y="ADDR_8" P=[%c%c%c%c%c%c%c%c] ("ADDR_16")="ADDR_8"\n",
			cpu.SP, cpu.A, cpu.X, cpu.Y, HILO(Negative), HILO(Overflow), HILO(Unused), HILO(BrkExecuted), HILO(Decimal), HILO(IrqDisabled), HILO(Zero), HILO(Carry),
			cpu.m_operandAddress, cpuRam.Read8(cpu.m_operandAddress));
#undef HILO

		// Some debugging facilities

		// Hardware breakpoints (on memory r/w)
		{
			static uint16 watchAddresses[10] = {0};
			auto iter = std::find(std::begin(watchAddresses), std::end(watchAddresses), cpu.m_operandAddress);
			if (*iter != 0 && iter != std::end(watchAddresses))
				System::DebugBreak();
		}

		// Regular breakpoints (on instruction just executed)
		{
			static uint16 breakpoints[10] = {0};
			auto iter = std::find(std::begin(breakpoints), std::end(breakpoints), cpu.m_lastPC);
			if (*iter != 0 && iter != std::end(breakpoints))
			{
				System::DebugBreak();
			}
		}
	}

private:
	Nes* m_nes;
};

namespace Debugger
{
	static DebuggerImpl g_debugger;

	void Initialize(Nes& nes) { g_debugger.Initialize(nes); }
	void Update() { g_debugger.Update(); }
	void PreCpuInstruction() { g_debugger.PreCpuInstruction(); }
	void PostCpuInstruction() { g_debugger.PostCpuInstruction(); }
}

#endif // DEBUGGING_ENABLED
