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
	void MemoryDump(T& memory, const char* file, uint32 from = 0x0000, uint32 to = 0x0000)
	{
		FileStream fs(file, "w");

		if (to == 0x0000)
			to = uint32((memory.MemorySize()) - 1);

		const size_t bytesPerLine = 16;
		size_t bytesPerLineCount = 0;

		uint8* pmem = memory.UnsafePtr((uint16)from);

		for (uint32 curr = from; curr <= to; ++curr)
		{
			if (bytesPerLineCount == 0)
			{
				fs.Printf("\n"ADDR_16": ", curr);
			}

			fs.Printf("%02X ", pmem[curr]);

			bytesPerLineCount = (bytesPerLineCount + 1) % bytesPerLine;
		}
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
					done = true;
					break;

				case 'q':
					throw std::exception("User quit from debugger");
					break;

				case 'g':
					stepMode = false;
					done = true;
					break;

				case 'd':
					{
						printf("Dumping: PpuRam.dmp\n");
						MemoryDump(m_nes->m_ppuRam, "PpuRam.dmp");
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
		printf(ADDR_16 "\t", PC);

		// Print instruction in hex
		for (uint16 i = 0; i < 4; ++i)
		{
			if (i < opCodeEntry.numBytes)
				printf("%02X", cpuRam.Read8(PC + i));
			else
				printf(" ");
		}
		printf("\t");

		// Print opcode name
		printf("%s ", OpCodeName::String[opCodeEntry.opCodeName]);

		// Print operand
		switch (opCodeEntry.addrMode)
		{
		case AddressMode::Immedt:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf("#" ADDR_8, address);
			}
			break;

		case AddressMode::Implid:
			// No operand to output
			break;

		case AddressMode::Accumu:
			{
				printf("A");
			}
			break;

		case AddressMode::Relatv:
			{
				// For branch instructions, resolve the target address and print it in comments
				const int8 offset = cpuRam.Read8(PC+1); // Signed offset in [-128,127]
				const uint16 target = PC + opCodeEntry.numBytes + offset;
				printf(ADDR_8 " ; " ADDR_16 " (%d)", (uint8)offset, target, offset);
			}
			break;

		case AddressMode::ZeroPg:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf(ADDR_8, address);
			}
			break;

		case AddressMode::ZPIdxX:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf(ADDR_8 ",X", address);
			}
			break;

		case AddressMode::ZPIdxY:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf(ADDR_8 ",Y", address);
			}
			break;

		case AddressMode::Absolu:
			{
				uint16 address = cpuRam.Read16(PC+1);
				printf(ADDR_16, address);
			}
			break;

		case AddressMode::AbIdxX:
			{
				uint16 address = cpuRam.Read16(PC+1);
				printf(ADDR_16 ",X", address);
			}
			break;

		case AddressMode::AbIdxY:
			{
				uint16 address = cpuRam.Read16(PC+1);
				printf(ADDR_16 ",Y", address);
			}
			break;

		case AddressMode::Indrct:
			{
				uint16 address = cpuRam.Read16(PC+1);
				printf("(" ADDR_16 ")", address);
			}
			break;

		case AddressMode::IdxInd:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf("(" ADDR_8 ",X)", address);
			}
			break;

		case AddressMode::IndIdx:
			{
				const uint8 address = cpuRam.Read8(PC+1);
				printf("(" ADDR_8 "),Y", address);
			}
			break;

		default:
			assert(false && "Invalid addressing mode");
			break;
		}

		printf("\n");
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

		printf("  SP="ADDR_8" A="ADDR_8" X="ADDR_8" Y="ADDR_8" P=[%c%c%c%c%c%c%c%c] ("ADDR_16")="ADDR_8"\n",
			cpu.SP, cpu.A, cpu.X, cpu.Y, HILO(Negative), HILO(Overflow), HILO(Unused), HILO(BrkExecuted), HILO(Decimal), HILO(IrqDisabled), HILO(Zero), HILO(Carry),
			cpu.m_operandAddress, cpuRam.Read8(cpu.m_operandAddress));

#undef HILO
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
