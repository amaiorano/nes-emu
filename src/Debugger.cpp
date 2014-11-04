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

#define FCEUX_OUTPUT 0

namespace
{
	bool g_trace = true;
	bool g_stepMode = false;
	uint16 g_instructionBreakpoints[10] = {0};
	uint16 g_dataBreakpoints[10] = {0};
}
#define TRACE printf

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

	void DumpMemory()
	{
		MemoryDumpPpuRam(m_nes->m_ppuRam);

		printf("Dumping: CpuRam.dmp\n");
		MemoryDump(m_nes->m_cpuRam, "CpuRam.dmp");

		printf("Dumping: SpriteRam.dmp\n");
		MemoryDump(m_nes->m_spriteRam, "SpriteRam.dmp");
	}

	void PreCpuInstruction()
	{
		if (g_trace)
		{
			PrintInstruction();
			PrintRegisters();
		#if !FCEUX_OUTPUT
			PrintOperandValue();
		#endif
			TRACE("\n");
		}

		ProcessBreakpoints();

		ProcessInput();
	}

	void PostCpuInstruction()
	{
	#if !FCEUX_OUTPUT
		if (g_trace)
		{
			TRACE("  Post: ");
			//PrintRegisters();
			PrintOperandValue();
			TRACE("\n");
		}
	#endif
	}

private:
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

	void ProcessInput()
	{
		const char KEY_SPACE = 32;
		const char KEY_ENTER = 13;

		char key;
		if (g_stepMode)
		{
			bool done = false;
			
			while (!done)
			{
				key = (char)tolower(System::WaitForKeyPress());
				//printf("key pressed: %c (%d)\n", key, key);

				switch (key)
				{
				case KEY_SPACE:
				case KEY_ENTER:
					done = true;
					if (!g_trace)
					{
						g_trace = true; // Turn back on in case it's off
						printf("[Trace on]\n");
					}
					break;

				case 'q':
					printf("[User Quit]\n");
					throw std::exception("User quit from debugger");
					break;

				case 'g':
					g_stepMode = false;					
					if (!g_trace)
						printf("[Running]\n");
					done = true;
					break;

				case 't':
					g_trace = !g_trace;
					printf("[Trace: %s]\n", g_trace? "on" : "off");
					break;

				case 'd':
					printf("[Dump Memory]\n");
					DumpMemory();
					break;
				}
			}
		}
		else if (System::GetKeyPress(key))
		{
			g_stepMode = true;

			if (!g_trace)
				printf("[Stopped]\n");
		}
	}

	void PrintRegisters()
	{
		Cpu& cpu = m_nes->m_cpu;

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
		#define ADDR_8_NO$ "%02X"

		TRACE("A:" ADDR_8_NO$ " X:" ADDR_8_NO$ " Y:" ADDR_8_NO$ " S:" ADDR_8_NO$ " P:%c%c%c%c%c%c%c%c",
			cpu.A, cpu.X, cpu.Y, cpu.SP, HILO(Negative), HILO(Overflow), HILO(Unused), HILO(BrkExecuted), HILO(Decimal), HILO(IrqDisabled), HILO(Zero), HILO(Carry) );

		#undef HILO
		#undef ADDR_8_NO$
	}

	void PrintOperandValue()
	{
		Cpu& cpu = m_nes->m_cpu;
		CpuRam& cpuRam = m_nes->m_cpuRam;
		TRACE(" (" ADDR_16 ")=" ADDR_8, cpu.m_operandAddress, cpuRam.Read8(cpu.m_operandAddress));
	}

	void PrintInstruction()
	{
		Cpu& cpu = m_nes->m_cpu;
		OpCodeEntry& opCodeEntry = *cpu.m_pEntry;
		CpuRam& cpuRam = m_nes->m_cpuRam;
		const uint16 PC = cpu.PC;
		const uint16 operandAddress = cpu.m_operandAddress; // Expected to be updated for current instruction

		// Print PC
		TRACE(ADDR_16 ":", PC);

		// Print instruction in hex
		for (uint16 i = 0; i < 3; ++i)
		{
			if (i < opCodeEntry.numBytes)
				TRACE("%02X ", cpuRam.Read8(PC + i));
			else
				TRACE("   ");
		}
		TRACE(" ");

		// Print opcode name
		TRACE("%s ", OpCodeName::String[opCodeEntry.opCodeName]);

		// Print operand
		char operandText[64] = {0};
		switch (opCodeEntry.addrMode)
		{
		case AddressMode::Immedt:
			{
				sprintf(operandText, "#" ADDR_8, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::Implid:
			// No operand to output
			break;

		case AddressMode::Accumu:
			{
			#if !FCEUX_OUTPUT
				sprintf(operandText, "A");
			#endif
			}
			break;

		case AddressMode::Relatv:
			{
				// For branch instructions, resolve the target address and print it in comments
			#if !FCEUX_OUTPUT
				const int8 offset = cpuRam.Read8(PC+1); // Signed offset in [-128,127]
				sprintf(operandText, ADDR_8 " ; " ADDR_16 " (%d)", (uint8)offset, operandAddress, offset);
			#else				
				sprintf(operandText, ADDR_16, operandAddress);
			#endif
			}
			break;

		case AddressMode::ZeroPg:
			{
				//@TODO: Do zero-page instructions really specify a 16 bit address: $00xx? This is what fceux outputs...
				sprintf(operandText, ADDR_16 " = #" ADDR_8, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::ZPIdxX:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				sprintf(operandText, ADDR_8 ",X @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::ZPIdxY:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				sprintf(operandText, ADDR_8 ",Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::Absolu:
			{
				const bool isJump = OpCodeName::String[opCodeEntry.opCodeName][0] == 'J';
				if (isJump)
					sprintf(operandText, ADDR_16, operandAddress);
				else
					sprintf(operandText, ADDR_16 " = #" ADDR_8, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::AbIdxX:
			{
				const uint16 address = cpuRam.Read16(PC+1);
				sprintf(operandText, ADDR_16 ",X @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::AbIdxY:
			{
				const uint16 address = cpuRam.Read16(PC+1);
				sprintf(operandText, ADDR_16 ",Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::Indrct:
			{
				const uint16 address = cpuRam.Read16(PC+1);
				sprintf(operandText, "(" ADDR_16 ") @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::IdxInd:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				sprintf(operandText, "(" ADDR_8 ",X) @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		case AddressMode::IndIdx:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				sprintf(operandText, "(" ADDR_8 "),Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpuRam.Read8(operandAddress));
			}
			break;

		default:
			assert(false && "Invalid addressing mode");
			break;
		}
		TRACE("%-41s", operandText);
	}

	void ProcessBreakpoints()
	{
		Cpu& cpu = m_nes->m_cpu;

		// Instruction breakpoints (before instruction executes)
		{
			auto iter = std::find(std::begin(g_instructionBreakpoints), std::end(g_instructionBreakpoints), cpu.PC);
			if (iter != std::end(g_instructionBreakpoints) && *iter != 0)
			{
				printf("[Instruction Breakpoint @ " ADDR_16 "]\n", *iter);
				if (!g_stepMode)
				{
					System::DebugBreak();
					g_stepMode = true;
				}
			}
		}

		// Data breakpoints (on memory r/w)
		{
			auto iter = std::find(std::begin(g_dataBreakpoints), std::end(g_dataBreakpoints), cpu.m_operandAddress);
			if (iter != std::end(g_dataBreakpoints) && *iter != 0)
			{
				printf("[Data breakpoint @ " ADDR_16 "]\n", *iter);
				if (!g_stepMode)
				{
					System::DebugBreak();
					g_stepMode = true;
				}
			}
		}
	}

	Nes* m_nes;
};

namespace Debugger
{
	static DebuggerImpl g_debugger;

	void Initialize(Nes& nes) { g_debugger.Initialize(nes); }
	void DumpMemory() { g_debugger.DumpMemory(); }
	void PreCpuInstruction() { g_debugger.PreCpuInstruction(); }
	void PostCpuInstruction() { g_debugger.PostCpuInstruction(); }
}

#endif // DEBUGGING_ENABLED
