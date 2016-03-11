#include "Cpu.h"
#include "Nes.h"
#include "OpCodeTable.h"
#include "MemoryMap.h"
#include "Serializer.h"
#include "Debugger.h"

// Some retail games overflow (on purpose?) like Battletoads
// so we can't leave this on
#define FAIL_ON_STACK_OVERFLOW 0

namespace
{
	OpCodeEntry** g_opCodeTable = GetOpCodeTable();

	FORCEINLINE uint16 GetPageAddress(uint16 address)
	{
		return (address & 0xFF00);
	}

	FORCEINLINE uint8 CalcNegativeFlag(uint16 v)
	{
		// Check if bit 7 is set
		return (v & 0x0080) != 0;
	}

	FORCEINLINE uint8 CalcNegativeFlag(uint8 v)
	{
		// Check if bit 7 is set
		return (v & 0x80) != 0;
	}

	FORCEINLINE uint8 CalcZeroFlag(uint16 v)
	{
		// Check that lower 8 bits are all 0
		return (v & 0x00FF) == 0;
	}

	FORCEINLINE uint8 CalcZeroFlag(uint8 v)
	{
		return v == 0;
	}

	FORCEINLINE uint8 CalcCarryFlag(uint16 v)
	{
		// Check if upper 8 bits are non-zero to know if a carry occured
		return (v & 0xFF00) != 0;
	}

	// Force link time error: need 16 bit to compute carry
	FORCEINLINE uint8 CalcCarryFlag(uint8 v);

	FORCEINLINE uint8 CalcOverflowFlag(uint8 a, uint8 b, uint16 r)
	{
		// With r = a + b, overflow occurs if both a and b are negative and r is positive,
		// or both a and b are positive and r is negative. Looking at sign bits of a, b, r,
		// overflow occurs when 0 0 1 or 1 1 0, so we can use simple xor logic to figure it out.
		return ((uint16)a ^ r) & ((uint16)b ^ r) & 0x0080;
	}

	// Force link time error: need 16 bit result to compute overflow
	FORCEINLINE uint8 CalcOverflowFlag(uint8 a, uint8 b, uint8 r);
}

Cpu::Cpu()
	: m_cpuMemoryBus(nullptr)
	, m_apu(nullptr)
	, m_opCodeEntry(nullptr)
{
}

void Cpu::Initialize(CpuMemoryBus& cpuMemoryBus, Apu& apu)
{
	m_cpuMemoryBus = &cpuMemoryBus;
	m_apu = &apu;

	m_controllerPorts.Initialize();
}

void Cpu::Reset()
{
	// See http://wiki.nesdev.com/w/index.php/CPU_power_up_state

	A = X = Y = 0;
	SP = 0xFF; // Should be FD, but for improved compatibility set to FF
	
	P.ClearAll();
	P.Set(StatusFlag::IrqDisabled);

	// Entry point is located at the Reset interrupt location
	PC = Read16(CpuMemory::kResetVector);

	m_cycles = 0;
	m_totalCycles = 0;	 
	m_pendingNmi = m_pendingIrq = false;

	m_controllerPorts.Reset();
}

void Cpu::Serialize(class Serializer& serializer)
{
	SERIALIZE(PC);
	SERIALIZE(SP);
	SERIALIZE(A);
	SERIALIZE(X);
	SERIALIZE(Y);
	SERIALIZE(P);
	SERIALIZE(m_cycles);
	SERIALIZE(m_totalCycles);
	SERIALIZE(m_pendingNmi);
	SERIALIZE(m_pendingIrq);
	SERIALIZE(m_spriteDmaRegister);
	serializer.SerializeObject(m_controllerPorts);
}

void Cpu::Nmi()
{
	assert(!m_pendingNmi && "Interrupt already pending");
	assert(!m_pendingIrq && "One interrupt at at time");
	m_pendingNmi = true;
}

void Cpu::Irq()
{
	assert(!m_pendingIrq && "Interrupt already pending");
	assert(!m_pendingNmi && "One interrupt at at time");

	if (!P.Test(StatusFlag::IrqDisabled))
		m_pendingIrq = true;
}

void Cpu::Execute(uint32& cpuCyclesElapsed)
{
	m_cycles = 0;
	
	ExecutePendingInterrupts(); // Handle when interrupts are called "between" CPU updates (e.g. PPU sends NMI)
	
	const uint8 opCode = Read8(PC);
	m_opCodeEntry = g_opCodeTable[opCode];

	if (m_opCodeEntry == nullptr)
	{
		FAIL("Unknown opcode");
	}

	UpdateOperandAddress();

	Debugger::PreCpuInstruction();
	ExecuteInstruction();
	ExecutePendingInterrupts(); // Handle when instruction (memory read) causes interrupt
	Debugger::PostCpuInstruction();		

	cpuCyclesElapsed = m_cycles;
	m_totalCycles += m_cycles;
}

uint8 Cpu::HandleCpuRead(uint16 cpuAddress)
{
	uint8 result = 0;

	switch (cpuAddress)
	{
	case CpuMemory::kSpriteDmaReg: // $4014
		result = m_spriteDmaRegister;
		break;

	case CpuMemory::kControllerPort1: // $4016
	case CpuMemory::kControllerPort2: // $4017
		result = m_controllerPorts.HandleCpuRead(cpuAddress);
		break;

	default:
		result = m_apu->HandleCpuRead(cpuAddress);
		break;
	}
	
	return result;
}

void Cpu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	switch (cpuAddress)
	{
	case CpuMemory::kSpriteDmaReg: // $4014
		{
			// Initiate a DMA transfer from the input page to sprite ram.

			static auto SpriteDmaTransfer = [&] (uint16 cpuAddress)
			{
				for (uint16 i = 0; i < 256; ++i) //@TODO: Use constant for 256 (kSpriteMemorySize?)
				{
					const uint8 value = m_cpuMemoryBus->Read(cpuAddress + i);
					m_cpuMemoryBus->Write(CpuMemory::kPpuSprRamIoReg, value);
				}

				// While DMA transfer occurs, the memory bus is in use, preventing CPU from fetching memory
				m_cycles += 512;
			};

			m_spriteDmaRegister = value;
			const uint16 srcCpuAddress = m_spriteDmaRegister * 0x100;

			// Note: we perform the full DMA transfer right here instead of emulating the transfers over multiple frames.
			// If we need to do it right, see http://wiki.nesdev.com/w/index.php/PPU_programmer_reference#DMA
			SpriteDmaTransfer(srcCpuAddress);

			return;
		}
		break;

	case CpuMemory::kControllerPort1: // $4016
		m_controllerPorts.HandleCpuWrite(cpuAddress, value);
		break;

	case CpuMemory::kControllerPort2: // $4017 For writes, this address is mapped to the APU!
	default:
		m_apu->HandleCpuWrite(cpuAddress, value);
		break;
	}
}

uint8 Cpu::Read8(uint16 address) const
{
	return m_cpuMemoryBus->Read(address);
}

uint16 Cpu::Read16(uint16 address) const
{
	return TO16(m_cpuMemoryBus->Read(address)) | (TO16(m_cpuMemoryBus->Read(address + 1)) << 8);
}

void Cpu::Write8(uint16 address, uint8 value)
{
	m_cpuMemoryBus->Write(address, value);
}

void Cpu::UpdateOperandAddress()
{
#if CONFIG_DEBUG
	m_operandAddress = 0; // Reset to help find bugs
#endif

	m_operandReadCrossedPage = false;

	switch (m_opCodeEntry->addrMode)
	{
	case AddressMode::Immedt:
		m_operandAddress = PC + 1; // Set to address of immediate value in code segment
		break;

	case AddressMode::Implid:
		break;

	case AddressMode::Accumu:
		break;

	case AddressMode::Relatv: // For conditional branch instructions
		{
			//@OPT: Lazily compute if branch condition succeeds

			// For branch instructions, resolve the target address
			const int8 offset = Read8(PC+1); // Signed offset in [-128,127]
			m_operandAddress = PC + m_opCodeEntry->numBytes + offset;
		}
		break;

	case AddressMode::ZeroPg:
		m_operandAddress = TO16(Read8(PC+1));
		break;

	case AddressMode::ZPIdxX:
		m_operandAddress = TO16((Read8(PC+1) + X)) & 0x00FF; // Wrap around zero-page boundary
		break;

	case AddressMode::ZPIdxY:
		m_operandAddress = TO16((Read8(PC+1) + Y)) & 0x00FF; // Wrap around zero-page boundary
		break;

	case AddressMode::Absolu:
		m_operandAddress = Read16(PC+1);
		break;

	case AddressMode::AbIdxX:
		{
			const uint16 baseAddress = Read16(PC+1);
			const uint16 basePage = GetPageAddress(baseAddress);
			m_operandAddress = baseAddress + X;
			m_operandReadCrossedPage = basePage != GetPageAddress(m_operandAddress);
		}
		break;

	case AddressMode::AbIdxY:
		{
			const uint16 baseAddress = Read16(PC+1);
			const uint16 basePage = GetPageAddress(baseAddress);
			m_operandAddress = baseAddress + Y;
			m_operandReadCrossedPage = basePage != GetPageAddress(m_operandAddress);
		}
		break;

	case AddressMode::Indrct: // for JMP only
		{
			uint16 low = Read16(PC+1);

			// Handle the 6502 bug for when the low-byte of the effective address is FF,
			// in which case the 2nd byte read does not correctly cross page boundaries.
			// The bug is that the high byte does not change.
			uint16 high = (low & 0xFF00) | ((low + 1) & 0x00FF);

			m_operandAddress = TO16(Read8(low)) | TO16(Read8(high)) << 8;
		}
		break;

	case AddressMode::IdxInd:
		{
			uint16 low = TO16((Read8(PC+1) + X)) & 0x00FF; // Zero page low byte of operand address, wrap around zero page
			uint16 high = TO16(low + 1) & 0x00FF; // Wrap high byte around zero page
			m_operandAddress = TO16(Read8(low)) | TO16(Read8(high)) << 8;
		}
		break;

	case AddressMode::IndIdx:
		{
			const uint16 low = TO16(Read8(PC+1)); // Zero page low byte of operand address
			const uint16 high = TO16(low + 1) & 0x00FF; // Wrap high byte around zero page
			const uint16 baseAddress = (TO16(Read8(low)) | TO16(Read8(high)) << 8);
			const uint16 basePage = GetPageAddress(baseAddress);
			m_operandAddress = baseAddress + Y;
			m_operandReadCrossedPage = basePage != GetPageAddress(m_operandAddress);
		}
		break;

	default:
		assert(false && "Invalid addressing mode");
		break;
	}
}

void Cpu::ExecuteInstruction()
{
	using namespace OpCodeName;
	using namespace StatusFlag;

	// By default, next instruction is after current, but can also be changed by a branch or jump
	uint16 nextPC = PC + m_opCodeEntry->numBytes;
	
	bool branchTaken = false;

	switch (m_opCodeEntry->opCodeName)
	{
	case ADC: // Add memory to accumulator with carry
		{
			// Operation:  A + M + C -> A, C
			const uint8 value = GetMemValue();
			const uint16 result = TO16(A) + TO16(value) + TO16(P.Test01(Carry));
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, CalcCarryFlag(result));
			P.Set(Overflow, CalcOverflowFlag(A, value, result));
			A = TO8(result);
		}
		break;

	case AND: // "AND" memory with accumulator
		A &= GetMemValue();
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case ASL: // Shift Left One Bit (Memory or Accumulator)
		{
			const uint16 result = TO16(GetAccumOrMemValue()) << 1;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, CalcCarryFlag(result));
			SetAccumOrMemValue(TO8(result));
		}
		break;

	case BCC: // Branch on Carry Clear
		if (!P.Test(Carry))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BCS: // Branch on Carry Set
		if (P.Test(Carry))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BEQ: // Branch on result zero (equal means compare difference is 0)
		if (P.Test(Zero))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BIT: // Test bits in memory with accumulator
		{
			uint8 memValue = GetMemValue();
			uint8 result = A & GetMemValue();
			P.SetValue( (P.Value() & 0x3F) | (memValue & 0xC0) ); // Copy bits 6 and 7 of mem value to status register
			P.Set(Zero, CalcZeroFlag(result));
		}
		break;

	case BMI: // Branch on result minus
		if (P.Test(Negative))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BNE:  // Branch on result non-zero
		if (!P.Test(Zero))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BPL: // Branch on result plus
		if (!P.Test(Negative))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BRK: // Force break (Forced Interrupt PC + 2 toS P toS) (used with RTI)
		{
			// Note that BRK is weird in that the instruction is 1 byte, but the return address
			// we store is 2 bytes after the instruction, so the byte after BRK will be skipped
			// upon return (RTI). Usually an NOP is inserted after a BRK for this reason.
			uint16 returnAddr = PC + 2;
			Push16(returnAddr);
			PushProcessorStatus(true);
			P.Set(IrqDisabled); // Disable hardware IRQs
			nextPC = Read16(CpuMemory::kIrqVector);
		}
		break;

	case BVC: // Branch on Overflow Clear
		if (!P.Test(Overflow))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case BVS: // Branch on Overflow Set
		if (P.Test(Overflow))
		{
			nextPC = GetBranchOrJmpLocation();
			branchTaken = true;
		}
		break;

	case CLC: // CLC Clear carry flag
		P.Clear(Carry);
		break;

	case CLD: // CLD Clear decimal mode
		P.Clear(Decimal);
		break;

	case CLI: // CLI Clear interrupt disable bit
		P.Clear(IrqDisabled);
		break;

	case CLV: // CLV Clear overflow flag
		P.Clear(Overflow);
		break;

	case CMP: // CMP Compare memory and accumulator
		{
			const uint8 memValue = GetMemValue();
			const uint8 result = A - memValue;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, A >= memValue); // Carry set if result positive or 0
		}
		break;

	case CPX: // CPX Compare Memory and Index X
		{
			const uint8 memValue = GetMemValue();
			const uint8 result = X - memValue;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, X >= memValue); // Carry set if result positive or 0
		}
		break;

	case CPY: // CPY Compare memory and index Y
		{
			const uint8 memValue = GetMemValue();
			const uint8 result = Y - memValue;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, Y >= memValue); // Carry set if result positive or 0
		}
		break;

	case DEC: // Decrement memory by one
		{
			const uint8 result = GetMemValue() - 1;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			SetMemValue(result);
		}
		break;

	case DEX: // Decrement index X by one
		--X;
		P.Set(Negative, CalcNegativeFlag(X));
		P.Set(Zero, CalcZeroFlag(X));
		break;

	case DEY: // Decrement index Y by one
		--Y;
		P.Set(Negative, CalcNegativeFlag(Y));
		P.Set(Zero, CalcZeroFlag(Y));
		break;

	case EOR: // "Exclusive-Or" memory with accumulator
		A = A ^ GetMemValue();
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case INC: // Increment memory by one
		{
			const uint8 result = GetMemValue() + 1;
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			SetMemValue(result);
		}
		break;

	case INX: // Increment Index X by one
		++X;
		P.Set(Negative, CalcNegativeFlag(X));
		P.Set(Zero, CalcZeroFlag(X));
		break;

	case INY: // Increment Index Y by one
		++Y;
		P.Set(Negative, CalcNegativeFlag(Y));
		P.Set(Zero, CalcZeroFlag(Y));
		break;

	case JMP: // Jump to new location
		nextPC = GetBranchOrJmpLocation();
		break;

	case JSR: // Jump to subroutine (used with RTS)
		{
			// JSR actually pushes address of the next instruction - 1.
			// RTS jumps to popped value + 1.
			const uint16 returnAddr = PC + m_opCodeEntry->numBytes - 1;
			Push16(returnAddr);
			nextPC = GetBranchOrJmpLocation();
		}
		break;

	case LDA: // Load accumulator with memory
		A = GetMemValue();
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case LDX: // Load index X with memory
		X = GetMemValue();
		P.Set(Negative, CalcNegativeFlag(X));
		P.Set(Zero, CalcZeroFlag(X));
		break;

	case LDY: // Load index Y with memory
		Y = GetMemValue();
		P.Set(Negative, CalcNegativeFlag(Y));
		P.Set(Zero, CalcZeroFlag(Y));
		break;

	case LSR: // Shift right one bit (memory or accumulator)
		{
			const uint8 value = GetAccumOrMemValue();
			const uint8 result = value >> 1;
			P.Set(Carry, value & 0x01); // Will get shifted into carry
			P.Set(Zero, CalcZeroFlag(result));
			P.Clear(Negative); // 0 is shifted into sign bit position
			SetAccumOrMemValue(result);
		}		
		break;

	case NOP: // No Operation (2 cycles)
		break;

	case ORA: // "OR" memory with accumulator
		A |= GetMemValue();
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case PHA: // Push accumulator on stack
		Push8(A);
		break;

	case PHP: // Push processor status on stack
		PushProcessorStatus(true);
		break;

	case PLA: // Pull accumulator from stack
		A = Pop8();
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case PLP: // Pull processor status from stack
		PopProcessorStatus();
		break;

	case ROL: // Rotate one bit left (memory or accumulator)
		{
			const uint16 result = (TO16(GetAccumOrMemValue()) << 1) | TO16(P.Test01(Carry));
			P.Set(Carry, CalcCarryFlag(result));
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			SetAccumOrMemValue(TO8(result));
		}
		break;

	case ROR: // Rotate one bit right (memory or accumulator)
		{
			const uint8 value = GetAccumOrMemValue();
			const uint8 result = (value >> 1) | (P.Test01(Carry) << 7);
			P.Set(Carry, value & 0x01);
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			SetAccumOrMemValue(result);
		}
		break;

	case RTI: // Return from interrupt (used with BRK, Nmi or Irq)
		{
			PopProcessorStatus();
			nextPC = Pop16();
		}
		break;

	case RTS: // Return from subroutine (used with JSR)
		{
			nextPC = Pop16() + 1;
		}
		break;

	case SBC: // Subtract memory from accumulator with borrow
		{
			// Operation:  A - M - C -> A

			// Can't simply negate mem value because that results in two's complement
			// and we want to perform the bitwise add ourself
			const uint8 value = GetMemValue() ^ 0XFF;

			const uint16 result = TO16(A) + TO16(value) + TO16(P.Test01(Carry));
			P.Set(Negative, CalcNegativeFlag(result));
			P.Set(Zero, CalcZeroFlag(result));
			P.Set(Carry, CalcCarryFlag(result));
			P.Set(Overflow, CalcOverflowFlag(A, value, result));
			A = TO8(result);
		}
		break;

	case SEC: // Set carry flag
		P.Set(Carry);
		break;

	case SED: // Set decimal mode
		P.Set(Decimal);
		break;

	case SEI: // Set interrupt disable status
		P.Set(IrqDisabled);
		break;

	case STA: // Store accumulator in memory
		SetMemValue(A);
		break;

	case STX: // Store index X in memory
		SetMemValue(X);
		break;

	case STY: // Store index Y in memory
		SetMemValue(Y);
		break;

	case TAX: // Transfer accumulator to index X
		X = A;
		P.Set(Negative, CalcNegativeFlag(X));
		P.Set(Zero, CalcZeroFlag(X));
		break;

	case TAY: // Transfer accumulator to index Y
		Y = A;
		P.Set(Negative, CalcNegativeFlag(Y));
		P.Set(Zero, CalcZeroFlag(Y));
		break;

	case TSX: // Transfer stack pointer to index X
		X = SP;
		P.Set(Negative, CalcNegativeFlag(X));
		P.Set(Zero, CalcZeroFlag(X));
		break;

	case TXA: // Transfer index X to accumulator
		A = X;
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	case TXS: // Transfer index X to stack pointer
		SP = X;
		break;

	case TYA: // Transfer index Y to accumulator
		A = Y;
		P.Set(Negative, CalcNegativeFlag(A));
		P.Set(Zero, CalcZeroFlag(A));
		break;

	default:
		assert(false);
		break;
	}

	// Compute cycles for instruction
	{
		uint16 cycles = m_opCodeEntry->numCycles;

		// Some instructions take an extra cycle when reading operand across page boundary
		if (m_operandReadCrossedPage)
			cycles += m_opCodeEntry->pageCrossCycles;

		// Extra cycle when branch is taken
		if (branchTaken)
		{
			++cycles;

			// And extra cycle when branching to a different page
			if (GetPageAddress(PC) != GetPageAddress(nextPC))
			{
				++cycles;
			}
		}

		m_cycles += cycles;
	}

	// Move to next instruction
	PC = nextPC;
}

void Cpu::ExecutePendingInterrupts()
{
	const uint16 kInterruptCycles = 7;

	if (m_pendingNmi)
	{
		Push16(PC);
		PushProcessorStatus(false);
		P.Clear(StatusFlag::BrkExecuted);
		P.Set(StatusFlag::IrqDisabled);
		PC = Read16(CpuMemory::kNmiVector);
		
		//@HACK: *2 here fixes Battletoads not loading levels, and also Marble Madness
		// not rendering start of level text box correctly. This is likely due to discrepencies
		// in cycle timing for when PPU signals an NMI and CPU handles it.
		m_cycles += kInterruptCycles * 2;
		
		m_pendingNmi = false;
	}
	else if (m_pendingIrq)
	{
		Push16(PC);
		PushProcessorStatus(false);
		P.Clear(StatusFlag::BrkExecuted);
		P.Set(StatusFlag::IrqDisabled);
		PC = Read16(CpuMemory::kIrqVector);
		m_cycles += kInterruptCycles;
		m_pendingIrq = false;
	}
}

uint8 Cpu::GetAccumOrMemValue() const
{
	assert(m_opCodeEntry->addrMode == AddressMode::Accumu || m_opCodeEntry->addrMode & AddressMode::MemoryValueOperand);

	if (m_opCodeEntry->addrMode == AddressMode::Accumu)
		return A;
	
	uint8 result = Read8(m_operandAddress);
	return result;
}

void Cpu::SetAccumOrMemValue(uint8 value)
{
	assert(m_opCodeEntry->addrMode == AddressMode::Accumu || m_opCodeEntry->addrMode & AddressMode::MemoryValueOperand);

	if (m_opCodeEntry->addrMode == AddressMode::Accumu)
	{
		A = value;
	}
	else
	{
		Write8(m_operandAddress, value);
	}
}

uint8 Cpu::GetMemValue() const
{
	assert(m_opCodeEntry->addrMode & AddressMode::MemoryValueOperand);
	uint8 result = Read8(m_operandAddress);
	return result;
}

void Cpu::SetMemValue(uint8 value)
{
	assert(m_opCodeEntry->addrMode & AddressMode::MemoryValueOperand);
	Write8(m_operandAddress, value);
}

uint16 Cpu::GetBranchOrJmpLocation() const
{
	assert(m_opCodeEntry->addrMode & AddressMode::JmpOrBranchOperand);
	return m_operandAddress;
}

void Cpu::Push8(uint8 value)
{
	Write8(CpuMemory::kStackBase + SP, value);
	--SP;
	
#if FAIL_ON_STACK_OVERFLOW
	if (SP == 0xFF)
	{
		FAIL("Stack overflow!");
	}
#endif
}

void Cpu::Push16(uint16 value)
{
	Push8(value >> 8);
	Push8(value & 0x00FF);
}

uint8 Cpu::Pop8()
{
	++SP;

#if FAIL_ON_STACK_OVERFLOW
	if (SP == 0)
	{
		FAIL("Stack underflow!");
	}
#endif

	return Read8(CpuMemory::kStackBase + SP);
}

uint16 Cpu::Pop16()
{
	const uint16 low = TO16(Pop8());
	const uint16 high = TO16(Pop8());
	return (high << 8) | low;
}

void Cpu::PushProcessorStatus(bool softwareInterrupt)
{
	assert(!P.Test(StatusFlag::Unused) && !P.Test(StatusFlag::BrkExecuted) && "P should never have these set, only on stack");
	uint8 brkFlag = softwareInterrupt? StatusFlag::BrkExecuted : 0;
	Push8(P.Value() | StatusFlag::Unused | brkFlag);
}

void Cpu::PopProcessorStatus()
{
	P.SetValue(Pop8() & ~StatusFlag::Unused & ~StatusFlag::BrkExecuted);
	assert(!P.Test(StatusFlag::Unused) && !P.Test(StatusFlag::BrkExecuted) && "P should never have these set, only on stack");
}
