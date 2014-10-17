#pragma once

#include "Base.h"

class CpuRam;
struct OpCodeEntry;

class Cpu
{
public:
	void Initialize(CpuRam& cpuRam);
	void Reset();
	void Run();

private:
	// Updates m_operandAddress for current instruction based on addressing mode. Operand data is assumed to be at PC + 1 if it exists.
	void UpdateOperand();

	// Executes current instruction, updates PC
	void ExecuteInstruction();

	void DebuggerPrintOp();
	void DebuggerPrintState();

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

	bool m_quit;
	CpuRam* m_pRam;
	OpCodeEntry* m_pEntry; // Current opcode entry
	
	// Registers - not using the usual m_ prefix because I find the code looks
	// more straightfoward when using the typical register names
	uint16 PC;	// Program counter
	uint8 SP;	// Stack pointer
	uint8 A;	// Accumulator
	uint8 X;	// X register
	uint8 Y;	// Y register

	struct StatusRegister
	{
		union
		{
			// NOTE: The bit ordering is platform-dependent. We assert the order is correct
			// at runtime.
			struct
			{
				uint8 C : 1;	// Carry flag
				uint8 Z : 1;	// Zero flag
				uint8 I : 1;	// Interrupt (IRQ) disabled
				uint8 D : 1;	// Decimal mode
				uint8 B : 1;	// BRK executed (IRQ/software interupt)
				uint8 U : 1;	// Unused
				uint8 V : 1;	// Overflow flag
				uint8 N : 1;	// Negative flag (aka Sign flag)
			};

			uint8 flags;
		};
	} P; // Processor status (flags)
	static_assert(sizeof(StatusRegister)==1, "StatusRegister must be 1 byte");

	// Operand address is either the operand's memory location, or the target for a branch or jmp
	uint16 m_operandAddress;
};
