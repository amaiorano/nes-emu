#pragma once

#include "Base.h"
#include "Bitfield.h"

class Nes;
class CpuRam;
struct OpCodeEntry;

namespace StatusFlag
{
	enum Type : uint8
	{
		Carry			= 0x01,
		Zero			= 0x02,
		InterruptsOff	= 0x04, // Interrupt (IRQ) disabled
		Decimal			= 0x08,
		BrkExecuted		= 0x10, // BRK executed (IRQ/software interupt)
		Unused			= 0x20,
		Overflow		= 0x40, // 'V'
		Negative		= 0x80, // aka Sign flag
	};		
}

class Cpu
{
public:
	void Initialize(Nes& nes, CpuRam& cpuRam);
	void Reset();
	void Run();

private:
	friend class DebuggerImpl;

	// Updates m_operandAddress for current instruction based on addressing mode. Operand data is assumed to be at PC + 1 if it exists.
	void UpdateOperand();

	// Executes current instruction, updates PC
	void ExecuteInstruction();

	// For instructions that work on accumulator (A) or memory location
	uint8 GetAccumOrMemValue() const;
	void SetAccumOrMemValue(uint8 value);

	// For instructions that work on memory location
	uint8 GetMemValue() const;
	void SetMemValue(uint8 value);

	// Returns the target location for branch or jmp instructions
	uint16 GetBranchOrJmpLocation() const;

	// Stack manipulation functions, modify SP
	void Push8(uint8 value);
	void Push16(uint16 value);
	uint8 Pop8();
	uint16 Pop16();

	// Data members

	Nes* m_pNes;
	CpuRam* m_pRam;
	OpCodeEntry* m_pEntry; // Current opcode entry
	
	// Registers - not using the usual m_ prefix because I find the code looks
	// more straightfoward when using the typical register names
	uint16 PC;		// Program counter
	uint8 SP;		// Stack pointer
	uint8 A;		// Accumulator
	uint8 X;		// X register
	uint8 Y;		// Y register
	Bitfield8 P;	// Processor status (flags)

	// Operand address is either the operand's memory location, or the target for a branch or jmp
	uint16 m_operandAddress;
};
