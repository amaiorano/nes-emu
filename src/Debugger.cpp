#include "Debugger.h"

#if DEBUGGING_ENABLED

#include "Base.h"
#include "Nes.h"
#include "OpCodeTable.h"
#include "System.h"
#include "Stream.h"
#include "Input.h"
#include <cassert>

#define FCEUX_OUTPUT 0
#define ENABLE_POST_TRACE 0

#if FCEUX_OUTPUT
	#undef ENABLE_POST_TRACE
	#define ENABLE_POST_TRACE 0
#endif

namespace
{
	bool g_trace = false;
	uint16 g_instructionBreakpoints[10] = {0};
	uint16 g_dataBreakpoints[10] = {0};

	namespace Trace
	{
		FILE* g_file = nullptr;
		bool g_firstOpen = true;
		char g_buffer[KB(8)]; // Enough to hold one line, must be flushed to file stream often
		char* g_curr = g_buffer;

		void Open()
		{
			if (g_firstOpen)
			{
				g_file = fopen("trace.log", "w+");
				g_firstOpen = false;
			}
			else
			{
				g_file = fopen("trace.log", "a+");
			}
		}
		
		void FlushToFileStream();

		void Close()
		{
			if (g_file)
			{
				FlushToFileStream();
				fclose(g_file);
				g_file = nullptr;
				g_curr = g_buffer;
			}
		}

		// Call often to make sure not to overflow fixed buffer. Does not necessarily update
		// the file on disk as the stream is buffered, and even if the buffer is flushed, the
		// OS usually implements delayed writes. Use FlushToDisk() to force it to flush all
		// contents to disk.
		void FlushToFileStream()
		{
			if (g_curr > g_buffer)
			{
				assert(g_curr <= g_buffer + ARRAYSIZE(g_buffer));
				if (g_file == nullptr)
					Open();

				fwrite(g_buffer, g_curr - g_buffer, 1, g_file);
				g_curr = g_buffer;
			}
		}

		void FlushToDisk()
		{
			Close();
		}
	}
}

#define TRACEF(...)		Trace::g_curr += sprintf(Trace::g_curr, __VA_ARGS__)
#define TRACE(text)		do { strcat(Trace::g_curr, text); Trace::g_curr += (ARRAYSIZE(text)-1); } while (false)


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

	void Shutdown()
	{
		Trace::Close();
	}

	void Update()
	{
		const bool prevTrace = g_trace;

		if (Input::KeyPressed(SDL_SCANCODE_T))
		{
			g_trace = !g_trace;
			printf("[Trace: %s]\n", g_trace? "on" : "off");
		}

		if (Input::KeyPressed(SDL_SCANCODE_D))
		{
			printf("[Dump Memory]\n");
			DumpMemory();
		}

		if (Input::KeyPressed(SDL_SCANCODE_F))
		{
			if (g_trace)
			{
				printf("[Flushing Trace]\n");
				Trace::FlushToDisk();
			}
		}

		// If trace stopped, close file to flush out contents
		if (prevTrace && !g_trace)
		{
			Trace::Close();
		}
	}

	void DumpMemory()
	{
		const std::string& dumpDir = System::GetAppDirectory() + std::string("dumps/");
		System::CreateDirectory(dumpDir.c_str());

		MemoryDumpPpu(dumpDir, m_nes->m_ppuMemoryBus);
		MemoryDumpCpu(dumpDir, m_nes->m_cpuMemoryBus);
	}

	void PreCpuInstruction()
	{
		if (g_trace)
		{
			PrintCycleCount();
			PrintInstruction();
			PrintRegisters();
		#if !FCEUX_OUTPUT
			PrintOperandValue();
			PrintStack();
		#endif
			TRACE("\n");
		}

		ProcessBreakpoints();
	}

	void PostCpuInstruction()
	{
	#if ENABLE_POST_TRACE
		if (g_trace)
		{
			TRACE("  Post: ");
			//PrintRegisters();
			PrintOperandValue();
			TRACE("\n");
		}
	#endif

		// Flush after every instruction to make sure we don't overflow the fixed size buffer
		Trace::FlushToFileStream();
	}

private:
	template <typename T>
	void MemoryDump(T& memoryBus, FileStream& fs, uint16 start = 0x0000, size_t size = 0, size_t bytesPerLine = 16)
	{
		if (size == 0)
			size = KB(64);

		const uint32 from = start;
		const uint32 to = from + size - 1;
		size_t bytesPerLineCount = 0;

		for (uint32 curr = from; curr <= to; ++curr)
		{
			if (bytesPerLineCount == 0)
			{
				if (curr != from)
					fs.Printf("\n");
				fs.Printf(ADDR_16": ", curr);
			}
			fs.Printf("%02X ", memoryBus.Read(static_cast<uint16>(curr)));
			bytesPerLineCount = (bytesPerLineCount + 1) % bytesPerLine;
		}		
		fs.Printf("\n");
	}

	template <typename T>
	void MemoryDump(T& memoryBus, const char* file, uint16 start = 0x0000, size_t size = 0)
	{
		FileStream fs(file, "w");
		MemoryDump(memoryBus, fs, start, size);
	}

	void MemoryDumpPpu(const std::string& dumpDir, PpuMemoryBus& ppuMemoryBus)
	{
		// Dump full memory
		std::string file = dumpDir + "PpuMemory.dmp";
		printf("Dumping: %s\n", file.c_str());
		MemoryDump(ppuMemoryBus, file.c_str());
		
		// Dump detailed break down
		file = dumpDir + "PpuMemory-detail.dmp";
		printf("Dumping: %s\n", file.c_str());

		FileStream fs(file.c_str(), "w");

		for (size_t i = 0; i < PpuMemory::kNumPatternTables; ++i)
		{
			if (i > 0)
				fs.Printf("\n");
			fs.Printf("Pattern Table %d (%d bytes)\n\n", i, PpuMemory::kPatternTableSize);
			MemoryDump(ppuMemoryBus, fs, PpuMemory::GetPatternTableAddress(i), PpuMemory::kPatternTableSize, 32*2);
		}

		for (size_t i = 0; i < PpuMemory::kNumMaxNameTables; ++i)
		{
			fs.Printf("\nName Table %d (%d bytes)\n\n", i, PpuMemory::kNameTableSize);
			MemoryDump(ppuMemoryBus, fs, PpuMemory::GetNameTableAddress(i), PpuMemory::kNameTableSize, 32);

			fs.Printf("\nAttribute Table %d (%d bytes)\n\n", i, PpuMemory::kAttributeTableSize);
			MemoryDump(ppuMemoryBus, fs, PpuMemory::GetAttributeTableAddress(i), PpuMemory::kAttributeTableSize, 32);
		}

		fs.Printf("\nImage Palette (%d bytes)\n\n", PpuMemory::kSinglePaletteSize);
		MemoryDump(ppuMemoryBus, fs, PpuMemory::kImagePalette, PpuMemory::kSinglePaletteSize);

		fs.Printf("\nSprite Palette (%d bytes)\n\n", PpuMemory::kSinglePaletteSize);
		MemoryDump(ppuMemoryBus, fs, PpuMemory::kImagePalette, PpuMemory::kSinglePaletteSize);
	}

	void MemoryDumpCpu(const std::string& dumpDir, CpuMemoryBus& cpuMemoryBus)
	{
		const std::string& file = dumpDir + "CpuMemory.dmp";
		printf("Dumping: %s\n", file.c_str());
		MemoryDump(cpuMemoryBus, file.c_str());
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

		TRACEF("A:" ADDR_8_NO$ " X:" ADDR_8_NO$ " Y:" ADDR_8_NO$ " S:" ADDR_8_NO$ " P:%c%c%c%c%c%c%c%c",
			cpu.A, cpu.X, cpu.Y, cpu.SP, HILO(Negative), HILO(Overflow), HILO(Unused), HILO(BrkExecuted), HILO(Decimal), HILO(IrqDisabled), HILO(Zero), HILO(Carry) );

		#undef HILO
		#undef ADDR_8_NO$
	}

	void PrintOperandValue()
	{
		Cpu& cpu = m_nes->m_cpu;
		TRACEF(" (" ADDR_16 ")=" ADDR_8, cpu.m_operandAddress, cpu.Read8(cpu.m_operandAddress));
	}

	void PrintStack()
	{
		Cpu& cpu = m_nes->m_cpu;
		TRACE(" Stack:[ ");
		for (int sp = 0xFF; sp > cpu.SP; --sp)
		{
			TRACEF("%02X ", cpu.Read8(CpuMemory::kStackBase + (uint8)sp));
		}
		TRACE("]");
	}

	void PrintCycleCount()
	{
		Cpu& cpu = m_nes->m_cpu;
		TRACEF("c%-12d", cpu.m_totalCycles);
	}

	void PrintInstruction()
	{
		Cpu& cpu = m_nes->m_cpu;
		OpCodeEntry& opCodeEntry = *cpu.m_opCodeEntry;
		const uint16 PC = cpu.PC;
		const uint16 operandAddress = cpu.m_operandAddress; // Expected to be updated for current instruction

		// Print current PRG 16K bank/page
	#if !FCEUX_OUTPUT
		if (PC > 0x8000)
			TRACEF("%02X:", m_nes->m_cartridge.GetPrgBankIndex16k(PC));
		else
			TRACE("  :");
	#endif

		// Print PC
		TRACEF(ADDR_16 ":", PC);

		// Print instruction in hex
		for (uint16 i = 0; i < 3; ++i)
		{
			if (i < opCodeEntry.numBytes)
				TRACEF("%02X ", cpu.Read8(PC + i));
			else
				TRACE("   ");
		}
		TRACE(" ");

		// Print opcode name
		TRACEF("%s ", OpCodeName::String[opCodeEntry.opCodeName]);

		// Print operand
		char operandText[64] = {0};
		switch (opCodeEntry.addrMode)
		{
		case AddressMode::Immedt:
			{
				sprintf(operandText, "#" ADDR_8, cpu.Read8(operandAddress));
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
				const int8 offset = cpu.Read8(PC+1); // Signed offset in [-128,127]
				sprintf(operandText, ADDR_8 " ; " ADDR_16 " (%d)", (uint8)offset, operandAddress, offset);
			#else				
				sprintf(operandText, ADDR_16, operandAddress);
			#endif
			}
			break;

		case AddressMode::ZeroPg:
			{
				//@TODO: Do zero-page instructions really specify a 16 bit address: $00xx? This is what fceux outputs...
				sprintf(operandText, ADDR_16 " = #" ADDR_8, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::ZPIdxX:
			{
				const uint8 address = cpu.Read8(PC+1);
				sprintf(operandText, ADDR_8 ",X @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::ZPIdxY:
			{
				const uint8 address = cpu.Read8(PC+1);
				sprintf(operandText, ADDR_8 ",Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::Absolu:
			{
				const bool isJump = OpCodeName::String[opCodeEntry.opCodeName][0] == 'J';
				if (isJump)
					sprintf(operandText, ADDR_16, operandAddress);
				else
					sprintf(operandText, ADDR_16 " = #" ADDR_8, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::AbIdxX:
			{
				const uint16 address = cpu.Read16(PC+1);
				sprintf(operandText, ADDR_16 ",X @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::AbIdxY:
			{
				const uint16 address = cpu.Read16(PC+1);
				sprintf(operandText, ADDR_16 ",Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::Indrct:
			{
				const uint16 address = cpu.Read16(PC+1);
				sprintf(operandText, "(" ADDR_16 ") @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::IdxInd:
			{
				const uint8 address = cpu.Read8(PC+1);
				sprintf(operandText, "(" ADDR_8 ",X) @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		case AddressMode::IndIdx:
			{
				const uint8 address = cpu.Read8(PC+1);
				sprintf(operandText, "(" ADDR_8 "),Y @ " ADDR_16 " = #" ADDR_8, address, operandAddress, cpu.Read8(operandAddress));
			}
			break;

		default:
			assert(false && "Invalid addressing mode");
			break;
		}
		TRACEF("%-41s", operandText);
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
				System::DebugBreak();
			}
		}

		// Data breakpoints (on memory r/w)
		{
			auto iter = std::find(std::begin(g_dataBreakpoints), std::end(g_dataBreakpoints), cpu.m_operandAddress);
			if (iter != std::end(g_dataBreakpoints) && *iter != 0)
			{
				printf("[Data breakpoint @ " ADDR_16 "]\n", *iter);
				System::DebugBreak();
			}
		}
	}

	Nes* m_nes;
};

namespace Debugger
{
	static DebuggerImpl g_debugger;
	static bool g_isExecuting;

	struct ScopedExecuting
	{
		ScopedExecuting() { g_isExecuting = true; }
		~ScopedExecuting() { g_isExecuting = false; }
	};

	void Initialize(Nes& nes) { g_debugger.Initialize(nes); }
	void Shutdown() { g_debugger.Shutdown(); }
	void Update() { ScopedExecuting se; g_debugger.Update(); }
	void DumpMemory() { ScopedExecuting se; g_debugger.DumpMemory(); }
	void PreCpuInstruction() { ScopedExecuting se; g_debugger.PreCpuInstruction(); }
	void PostCpuInstruction() { ScopedExecuting se; g_debugger.PostCpuInstruction(); }
	bool IsExecuting() { return g_isExecuting; }
}

#else

namespace Debugger
{
	void Shutdown() {}
}

#endif // DEBUGGING_ENABLED

