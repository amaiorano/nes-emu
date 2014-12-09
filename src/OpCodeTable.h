#pragma once

#include "Base.h"
#include <cstring>

namespace AddressMode
{
	enum Type
	{
		Immedt = 0x0001, // Immediate : #value
		Implid = 0x0002, // Implied : no operand
		Accumu = 0x0004, // Accumulator : no operand
		Relatv = 0x0008, // Relative : $addr8 used with branch instructions
		ZeroPg = 0x0010, // Zero Page : $addr8
		ZPIdxX = 0x0020, // Zero Page Indexed with X : $addr8 + X
		ZPIdxY = 0x0040, // Zero Page Indexed with Y : $addr8 + Y
		Absolu = 0x0080, // Absolute : $addr16
		AbIdxX = 0x0100, // Absolute Indexed with X : $addr16 + X
		AbIdxY = 0x0200, // Absolute Indexed with Y : $addr16 + Y
		Indrct = 0x0400, // Indirect : ($addr8) used only with JMP
		IdxInd = 0x0800, // Indexed with X Indirect : ($addr8 + X)
		IndIdx = 0x1000, // Indirect Indexed with Y : ($addr8) + Y
	};

	const uint32 MemoryValueOperand = Immedt|ZeroPg|ZPIdxX|ZPIdxY|Absolu|AbIdxX|AbIdxY|IdxInd|IndIdx;
	const uint32 JmpOrBranchOperand = Relatv|Absolu|Indrct;
}
static_assert(sizeof(AddressMode::Type) == 4, "Should be 4 byte enum type");

namespace OpCodeName
{
	enum Type
	{
		ADC, AND, ASL,
		BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS,
		CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY,
		EOR, INC, INX, INY,
		JMP, JSR,
		LDA, LDX, LDY, LSR,
		NOP,
		ORA,
		PHA, PHP, PLA, PLP,
		ROL, ROR, RTI, RTS,
		SBC, SEC, SED, SEI, STA, STX, STY,
		TAX, TAY, TSX, TXA, TXS, TYA,

		NumTypes
	};

	static const char* String[] =
	{
		"ADC", "AND", "ASL",
		"BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS",
		"CLC", "CLD", "CLI", "CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY",
		"EOR", "INC", "INX", "INY",
		"JMP", "JSR",
		"LDA", "LDX", "LDY", "LSR",
		"NOP",
		"ORA",
		"PHA", "PHP", "PLA", "PLP",
		"ROL", "ROR", "RTI", "RTS",
		"SBC", "SEC", "SED", "SEI", "STA", "STX", "STY",
		"TAX", "TAY", "TSX", "TXA", "TXS", "TYA",
	};

	static_assert(NumTypes == ARRAYSIZE(String), "Size mismatch");
}

struct OpCodeEntry
{
	uint8 opCode;
	OpCodeName::Type opCodeName;
	uint8 numBytes;
	uint8 numCycles;
	uint8 pageCrossCycles; // 0 or 1
	AddressMode::Type addrMode;
};

// Returns the opcode table
OpCodeEntry** GetOpCodeTable();
