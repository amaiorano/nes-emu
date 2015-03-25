#pragma once

#include "Base.h"
#include "Bitfield.h"
#include "ControllerPorts.h"

class CpuMemoryBus;
class Apu;
struct OpCodeEntry;

namespace StatusFlag
{
	enum Type : uint8
	{
		Carry			= BIT(0),
		Zero			= BIT(1),
		IrqDisabled		= BIT(2), // Interrupt (IRQ) disabled
		Decimal			= BIT(3), // *NOTE: Present in P, but Decimal mode not supported by NES CPU
		BrkExecuted		= BIT(4), // BRK executed (IRQ/software interupt) *NOTE: Not actually a bit in P, only set on stack for s/w interrupts
		Unused			= BIT(5), // *NOTE: Never set in P, but always set on stack
		Overflow		= BIT(6), // 'V'
		Negative		= BIT(7), // aka Sign flag
	};
}

class Cpu
{
public:
	Cpu();
	void Initialize(CpuMemoryBus& cpuMemoryBus, Apu& apu);

	void Reset();
	void Serialize(class Serializer& serializer);

	void Nmi();
	void Irq();

	void Execute(uint32& cpuCyclesElapsed);

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);

private:
	friend class DebuggerImpl;

	uint8 Read8(uint16 address) const;
	uint16 Read16(uint16 address) const;
	void Write8(uint16 address, uint8 value);

	// Updates m_operandAddress for current instruction based on addressing mode. Operand data is assumed to be at PC + 1 if it exists.
	void UpdateOperandAddress();

	// Executes current instruction and updates PC
	void ExecuteInstruction();

	// Executes pending interrupts (if any)
	void ExecutePendingInterrupts();

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
	void PushProcessorStatus(bool softwareInterrupt);
	void PopProcessorStatus();

	// Data members

	CpuMemoryBus* m_cpuMemoryBus;
	Apu* m_apu;
	OpCodeEntry* m_opCodeEntry; // Current opcode entry
	
	// Registers - not using the usual m_ prefix because I find the code looks
	// more straightforward when using the typical register names
	uint16 PC;		// Program counter
	uint8 SP;		// Stack pointer
	uint8 A;		// Accumulator
	uint8 X;		// X register
	uint8 Y;		// Y register
	Bitfield8 P;	// Processor status (flags)

	uint16 m_cycles; // Elapsed cycles of each fetch and execute of an instruction
	uint64 m_totalCycles;

	bool m_pendingNmi;
	bool m_pendingIrq;

	// Operand address is either the operand's memory location, or the target for a branch or jmp
	uint16 m_operandAddress;
	bool m_operandReadCrossedPage;

	uint8 m_spriteDmaRegister; // $4014

	ControllerPorts m_controllerPorts;
};
