#include "OpCodeTable.h"

void ValidateOpCodeTable(OpCodeEntry opCodeTable[], size_t numEntries);

OpCodeEntry** GetOpCodeTable()
{
	using namespace OpCodeName;
	using namespace AddressMode;

	static OpCodeEntry opCodeTable[] =
	{
		{ 0x69, ADC, 2, 2, 0, Immedt },
		{ 0x65, ADC, 2, 3, 0, ZeroPg },
		{ 0x75, ADC, 2, 4, 0, ZPIdxX },
		{ 0x6D, ADC, 3, 4, 0, Absolu },
		{ 0x7D, ADC, 3, 4, 1, AbIdxX },
		{ 0x79, ADC, 3, 4, 1, AbIdxY },
		{ 0x61, ADC, 2, 6, 0, IdxInd },
		{ 0x71, ADC, 2, 5, 1, IndIdx },

		{ 0x29, AND, 2, 2, 0, Immedt },
		{ 0x25, AND, 2, 3, 0, ZeroPg },
		{ 0x35, AND, 2, 4, 0, ZPIdxX },
		{ 0x2D, AND, 3, 4, 0, Absolu },
		{ 0x3D, AND, 3, 4, 1, AbIdxX },
		{ 0x39, AND, 3, 4, 1, AbIdxY },
		{ 0x21, AND, 2, 6, 0, IdxInd },
		{ 0x31, AND, 2, 5, 1, IndIdx },

		{ 0x0A, ASL, 1, 2, 0, Accumu },
		{ 0x06, ASL, 2, 5, 0, ZeroPg },
		{ 0x16, ASL, 2, 6, 0, ZPIdxX },
		{ 0x0E, ASL, 3, 6, 0, Absolu },
		{ 0x1E, ASL, 3, 7, 0, AbIdxX },

		{ 0x90, BCC, 2, 2, 0, Relatv },
		{ 0xB0, BCS, 2, 2, 0, Relatv },
		{ 0xF0, BEQ, 2, 2, 0, Relatv },
		{ 0x24, BIT, 2, 3, 0, ZeroPg },
		{ 0x2C, BIT, 3, 4, 0, Absolu },
		{ 0x30, BMI, 2, 2, 0, Relatv },
		{ 0xD0, BNE, 2, 2, 0, Relatv },
		{ 0x10, BPL, 2, 2, 0, Relatv },
		{ 0x00, BRK, 1, 7, 0, Implid },
		{ 0x50, BVC, 2, 2, 0, Relatv },
		{ 0x70, BVS, 2, 2, 0, Relatv },

		{ 0x18, CLC, 1, 2, 0, Implid },
		{ 0xD8, CLD, 1, 2, 0, Implid },
		{ 0x58, CLI, 1, 2, 0, Implid },
		{ 0xB8, CLV, 1, 2, 0, Implid },

		{ 0xC9, CMP, 2, 2, 0, Immedt },
		{ 0xC5, CMP, 2, 3, 0, ZeroPg },
		{ 0xD5, CMP, 2, 4, 0, ZPIdxX },
		{ 0xCD, CMP, 3, 4, 0, Absolu },
		{ 0xDD, CMP, 3, 4, 1, AbIdxX },
		{ 0xD9, CMP, 3, 4, 1, AbIdxY },
		{ 0xC1, CMP, 2, 6, 0, IdxInd },
		{ 0xD1, CMP, 2, 5, 1, IndIdx },

		{ 0xE0, CPX, 2, 2, 0, Immedt },
		{ 0xE4, CPX, 2, 3, 0, ZeroPg },
		{ 0xEC, CPX, 3, 4, 0, Absolu },

		{ 0xC0, CPY, 2, 2, 0, Immedt },
		{ 0xC4, CPY, 2, 3, 0, ZeroPg },
		{ 0xCC, CPY, 3, 4, 0, Absolu },

		{ 0xC6, DEC, 2, 5, 0, ZeroPg },
		{ 0xD6, DEC, 2, 6, 0, ZPIdxX },
		{ 0xCE, DEC, 3, 6, 0, Absolu },
		{ 0xDE, DEC, 3, 7, 0, AbIdxX },

		{ 0xCA, DEX, 1, 2, 0, Implid },

		{ 0x88, DEY, 1, 2, 0, Implid },

		{ 0x49, EOR, 2, 2, 0, Immedt },
		{ 0x45, EOR, 2, 3, 0, ZeroPg },
		{ 0x55, EOR, 2, 4, 0, ZPIdxX },
		{ 0x4D, EOR, 3, 4, 0, Absolu },
		{ 0x5D, EOR, 3, 4, 1, AbIdxX },
		{ 0x59, EOR, 3, 4, 1, AbIdxY },
		{ 0x41, EOR, 2, 6, 0, IdxInd },
		{ 0x51, EOR, 2, 5, 1, IndIdx },

		{ 0xE6, INC, 2, 5, 0, ZeroPg },
		{ 0xF6, INC, 2, 6, 0, ZPIdxX },
		{ 0xEE, INC, 3, 6, 0, Absolu },
		{ 0xFE, INC, 3, 7, 0, AbIdxX },

		{ 0xE8, INX, 1, 2, 0, Implid },
		{ 0xC8, INY, 1, 2, 0, Implid },

		{ 0x4C, JMP, 3, 3, 0, Absolu },
		{ 0x6C, JMP, 3, 5, 0, Indrct },
		{ 0x20, JSR, 3, 6, 0, Absolu },

		{ 0xA9, LDA, 2, 2, 0, Immedt },
		{ 0xA5, LDA, 2, 3, 0, ZeroPg },
		{ 0xB5, LDA, 2, 4, 0, ZPIdxX },
		{ 0xAD, LDA, 3, 4, 0, Absolu },
		{ 0xBD, LDA, 3, 4, 1, AbIdxX },
		{ 0xB9, LDA, 3, 4, 1, AbIdxY },
		{ 0xA1, LDA, 2, 6, 0, IdxInd },
		{ 0xB1, LDA, 2, 5, 1, IndIdx },

		{ 0xA2, LDX, 2, 2, 0, Immedt },
		{ 0xA6, LDX, 2, 3, 0, ZeroPg },
		{ 0xB6, LDX, 2, 4, 0, ZPIdxY },
		{ 0xAE, LDX, 3, 4, 0, Absolu },
		{ 0xBE, LDX, 3, 4, 1, AbIdxY },

		{ 0xA0, LDY, 2, 2, 0, Immedt },
		{ 0xA4, LDY, 2, 3, 0, ZeroPg },
		{ 0xB4, LDY, 2, 4, 0, ZPIdxX },
		{ 0xAC, LDY, 3, 4, 0, Absolu },
		{ 0xBC, LDY, 3, 4, 1, AbIdxX },

		{ 0x4A, LSR, 1, 2, 0, Accumu },
		{ 0x46, LSR, 2, 5, 0, ZeroPg },
		{ 0x56, LSR, 2, 6, 0, ZPIdxX },
		{ 0x4E, LSR, 3, 6, 0, Absolu },
		{ 0x5E, LSR, 3, 7, 0, AbIdxX },
		
		{ 0xEA, NOP, 1, 2, 0, Implid },

		{ 0x09, ORA, 2, 2, 0, Immedt },
		{ 0x05, ORA, 2, 3, 0, ZeroPg },
		{ 0x15, ORA, 2, 4, 0, ZPIdxX },
		{ 0x0D, ORA, 3, 4, 0, Absolu },
		{ 0x1D, ORA, 3, 4, 1, AbIdxX },
		{ 0x19, ORA, 3, 4, 1, AbIdxY },
		{ 0x01, ORA, 2, 6, 0, IdxInd },
		{ 0x11, ORA, 2, 5, 1, IndIdx },

		{ 0x48, PHA, 1, 3, 0, Implid },
		{ 0x08, PHP, 1, 3, 0, Implid },
		{ 0x68, PLA, 1, 4, 0, Implid },
		{ 0x28, PLP, 1, 4, 0, Implid },
		
		{ 0x2A, ROL, 1, 2, 0, Accumu },
		{ 0x26, ROL, 2, 5, 0, ZeroPg },
		{ 0x36, ROL, 2, 6, 0, ZPIdxX },
		{ 0x2E, ROL, 3, 6, 0, Absolu },
		{ 0x3E, ROL, 3, 7, 0, AbIdxX },
		
		{ 0x6A, ROR, 1, 2, 0, Accumu },
		{ 0x66, ROR, 2, 5, 0, ZeroPg },
		{ 0x76, ROR, 2, 6, 0, ZPIdxX },
		{ 0x6E, ROR, 3, 6, 0, Absolu },
		{ 0x7E, ROR, 3, 7, 0, AbIdxX },
		
		{ 0x40, RTI, 1, 6, 0, Implid },
		{ 0x60, RTS, 1, 6, 0, Implid },

		{ 0xE9, SBC, 2, 2, 0, Immedt },
		{ 0xE5, SBC, 2, 3, 0, ZeroPg },
		{ 0xF5, SBC, 2, 4, 0, ZPIdxX },
		{ 0xED, SBC, 3, 4, 0, Absolu },
		{ 0xFD, SBC, 3, 4, 1, AbIdxX },
		{ 0xF9, SBC, 3, 4, 1, AbIdxY },
		{ 0xE1, SBC, 2, 6, 0, IdxInd },
		{ 0xF1, SBC, 2, 5, 1, IndIdx },

		{ 0x38, SEC, 1, 2, 0, Implid },
		{ 0xF8, SED, 1, 2, 0, Implid },
		{ 0x78, SEI, 1, 2, 0, Implid },
		
		{ 0x85, STA, 2, 3, 0, ZeroPg },
		{ 0x95, STA, 2, 4, 0, ZPIdxX },
		{ 0x8D, STA, 3, 4, 0, Absolu },
		{ 0x9D, STA, 3, 5, 0, AbIdxX },
		{ 0x99, STA, 3, 5, 0, AbIdxY },
		{ 0x81, STA, 2, 6, 0, IdxInd },
		{ 0x91, STA, 2, 6, 0, IndIdx },
		
		{ 0x86, STX, 2, 3, 0, ZeroPg },
		{ 0x96, STX, 2, 4, 0, ZPIdxY },
		{ 0x8E, STX, 3, 4, 0, Absolu },
		
		{ 0x84, STY, 2, 3, 0, ZeroPg },
		{ 0x94, STY, 2, 4, 0, ZPIdxX },
		{ 0x8C, STY, 3, 4, 0, Absolu },
		
		{ 0xAA, TAX, 1, 2, 0, Implid },
		{ 0xA8, TAY, 1, 2, 0, Implid },
		{ 0xBA, TSX, 1, 2, 0, Implid },
		{ 0x8A, TXA, 1, 2, 0, Implid },
		{ 0x9A, TXS, 1, 2, 0, Implid },
		{ 0x98, TYA, 1, 2, 0, Implid },
	};

	static OpCodeEntry* opCodeTableOrdered[256];
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		ValidateOpCodeTable(opCodeTable, ARRAYSIZE(opCodeTable));
		
		memset(opCodeTableOrdered, 0, sizeof(opCodeTableOrdered));
		for (size_t i = 0; i < ARRAYSIZE(opCodeTable); ++i)
		{
			const uint8 opCode = opCodeTable[i].opCode;
			assert(opCodeTableOrdered[opCode] == 0 && "Error in table: opCode collision");
			opCodeTableOrdered[opCode] = &opCodeTable[i];
		}
	}

	return opCodeTableOrdered;
}

void ValidateOpCodeTable(OpCodeEntry opCodeTable[], size_t numEntries)
{
	for (size_t i = 0; i < numEntries; ++i)
	{
		const OpCodeEntry& entry = opCodeTable[i];
		(void)entry;
		switch (opCodeTable[i].addrMode)
		{
		case AddressMode::Immedt:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::Implid:
			assert(entry.numBytes == 1);
			break;
		case AddressMode::Accumu:
			assert(entry.numBytes == 1);
			break;
		case AddressMode::Relatv:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::ZeroPg:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::ZPIdxX:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::ZPIdxY:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::Absolu:
			assert(entry.numBytes == 3);
			break;
		case AddressMode::AbIdxX:
			assert(entry.numBytes == 3);
			break;
		case AddressMode::AbIdxY:
			assert(entry.numBytes == 3);
			break;
		case AddressMode::Indrct:
			assert(entry.numBytes == 3);
			assert(entry.opCodeName == OpCodeName::JMP);
			break;
		case AddressMode::IdxInd:
			assert(entry.numBytes == 2);
			break;
		case AddressMode::IndIdx:
			assert(entry.numBytes == 2);
			break;
		}
	}
}
