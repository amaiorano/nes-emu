#include "Cpu.h"
#include "CpuRam.h"
#include "OpCodeTable.h"
#include <conio.h>

#define ADDR_8 "$%02X"
#define ADDR_16 "$%04X"
#define TO16(v8) ((uint16)(v8))
#define TO8(v16) ((uint8)(v16 & 0x00FF))

#define DEBUGGING_ENABLED 1

namespace
{
	OpCodeEntry** g_ppOpCodeTable = GetOpCodeTable();

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

void Cpu::Initialize(CpuRam& cpuRam)
{
	m_pRam = &cpuRam;
}

void Cpu::Reset()
{
	// See http://wiki.nesdev.com/w/index.php/CPU_power_up_state

	A = X = Y = 0;
	SP = 0x0FD;
	
	P.flags = 0;
	P.Bit5 = 1;
	P.B = 1;
	P.I = 1;

	// Entry point is located at the Reset interrupt location
	PC = m_pRam->Read16(CpuRam::kResetVector);
}

static void Disassemble(uint16 PC, OpCodeEntry* m_pEntry, const CpuRam* pRam)
{
	// Print PC
	printf(ADDR_16 "\t", PC);

	// Print instruction in hex
	for (uint16 i = 0; i < 4; ++i)
	{
		if (i < m_pEntry->numBytes)
			printf("%02X", pRam->Read8(PC + i));
		else
			printf(" ");
	}
	printf("\t");

	// Print opcode name
	printf("%s ", OpCodeName::String[m_pEntry->opCodeName]);

	// Print operand
	switch (m_pEntry->addrMode)
	{
	case AddressMode::Immedt:
		{
			const uint8 address = pRam->Read8(PC+1);
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
			const int8 offset = pRam->Read8(PC+1); // Signed offset in [-128,127]
			const uint16 target = PC + m_pEntry->numBytes + offset;
			printf(ADDR_8 " ; " ADDR_16 " (%d)", offset, target, offset);
		}
		break;

	case AddressMode::ZeroPg:
		{
			const uint8 address = pRam->Read8(PC+1);
			printf(ADDR_8, address);
		}
		break;

	case AddressMode::ZPIdxX:
		{
			const uint8 address = pRam->Read8(PC+1);
			printf(ADDR_8 ",X", address);
		}
		break;

	case AddressMode::ZPIdxY:
		{
			const uint8 address = pRam->Read8(PC+1);
			printf(ADDR_8 ",Y", address);
		}
		break;

	case AddressMode::Absolu:
		{
			uint16 address = pRam->Read16(PC+1);
			printf(ADDR_16, address);
		}
		break;

	case AddressMode::AbIdxX:
		{
			uint16 address = pRam->Read16(PC+1);
			printf(ADDR_16 ",X", address);
		}
		break;

	case AddressMode::AbIdxY:
		{
			uint16 address = pRam->Read16(PC+1);
			printf(ADDR_16 ",Y", address);
		}
		break;

	case AddressMode::Indrct:
		{
			uint16 address = pRam->Read16(PC+1);
			printf("(" ADDR_16 ")", address);
		}
		break;

	case AddressMode::IdxInd:
		{
			const uint8 address = pRam->Read8(PC+1);
			printf("(" ADDR_8 ",X)", address);
		}
		break;

	case AddressMode::IndIdx:
		{
			const uint8 address = pRam->Read8(PC+1);
			printf("(" ADDR_8 "),Y", address);
		}
		break;

	default:
		assert(false && "Invalid addressing mode");
		break;
	}

	printf("\n");
}

void Cpu::Run()
{
	bool quit = false;
	while (!quit)
	{
		uint8 opCode = m_pRam->Read8(PC);
		m_pEntry = g_ppOpCodeTable[opCode];

		if (m_pEntry == nullptr)
		{
			assert(false && "Unknown opcode");
		}

#if DEBUGGING_ENABLED
		Disassemble(PC, m_pEntry, m_pRam);
		while (!_kbhit()) {}
		_getch();
#endif

		UpdateOperand();

		ExecuteInstruction();
	}
}

void Cpu::UpdateOperand()
{
	//@TODO: For all 2 byte reads, we need to compute potential page boundary penalty
	
	//@OPT: The first read from memory always reads from code segment, so there's no need to worry
	// about mirroring. Either provide a faster CpuRam::Read func, or just get a pointer to the start
	// of the code segment.

	switch (m_pEntry->addrMode)
	{
	case AddressMode::Immedt:
		m_operandAddress = m_pRam->Read8(PC+1);
		break;

	case AddressMode::Implid:
		break;

	case AddressMode::Accumu:
		break;

	case AddressMode::Relatv: // For conditional branch instructions
		{
			//@OPT: Lazily compute if branch condition succeeds

			// For branch instructions, resolve the target address
			const int8 offset = m_pRam->Read8(PC+1); // Signed offset in [-128,127]
			m_operandAddress = PC + m_pEntry->numBytes + offset;
		}
		break;

	case AddressMode::ZeroPg:
		m_operandAddress = TO16(m_pRam->Read8(PC+1));
		break;

	case AddressMode::ZPIdxX:
		m_operandAddress = TO16((m_pRam->Read8(PC+1) + X)) & 0x00FF; // Wrap around zero-page boundary
		break;

	case AddressMode::ZPIdxY:
		m_operandAddress = TO16((m_pRam->Read8(PC+1) + Y)) & 0x00FF; // Wrap around zero-page boundary
		break;

	case AddressMode::Absolu:
		m_operandAddress = m_pRam->Read16(PC+1);
		break;

	case AddressMode::AbIdxX:
		m_operandAddress = m_pRam->Read16(PC+1) + X;
		break;

	case AddressMode::AbIdxY:
		m_operandAddress = m_pRam->Read16(PC+1) + Y;
		break;

	case AddressMode::Indrct: // for JMP only
		{
			const uint16 indirectAddress = m_pRam->Read16(PC+1);
			m_operandAddress = m_pRam->Read16(indirectAddress);
		}
		break;

	case AddressMode::IdxInd:
		{
			uint16 indirectAddress = TO16((m_pRam->Read8(PC+1) + X)) & 0x00FF; // Get zero page address of bytes holding address of operand
			m_operandAddress = m_pRam->Read16(indirectAddress); // Get operand address
		}
		break;

	case AddressMode::IndIdx:
		{
			const uint16 indirectAddress = TO16(m_pRam->Read8(PC+1)); // Get zero page address of bytes holding address of operand
			m_operandAddress = m_pRam->Read16(indirectAddress) + Y; // Get operand address
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

	const uint16 startPC = PC;

	switch (m_pEntry->opCodeName)
	{
	case ADC: // Add memory to accumulator with carry
		{
			// Operation:  A + M + C -> A, C              N Z C I D V
			//                                            / / / _ _ /
			uint8 value = GetAccumOrMemValue();
			uint16 result = TO16(A) + TO16(value) + TO16(P.C);
			P.N = CalcNegativeFlag(result);
			P.Z = CalcZeroFlag(result);
			P.C = CalcCarryFlag(result);
			P.V = CalcOverflowFlag(A, value, result);
			A = TO8(result);
		}
		break;

	case AND: // "AND" memory with accumulator
		{
			// Operation:  A /\ M -> A                    N Z C I D V
			//                                            / / _ _ _ _
			A &= GetMemValue();
			P.N = CalcNegativeFlag(A);
			P.Z = CalcZeroFlag(A);
		}
		break;

	case ASL: // Shift Left One Bit (Memory or Accumulator)
		{
			// Operation:  C <- |7|6|5|4|3|2|1|0| <- 0    N Z C I D V
			//                                            / / / _ _ _
			uint16 result = TO16(GetAccumOrMemValue()) << 1;
			P.N = CalcNegativeFlag(result);
			P.Z = CalcZeroFlag(result);
			P.C = CalcCarryFlag(result);
			SetAccumOrMemValue(TO8(result));
		}
		break;

	case BCC: // Branch on Carry Clear
		if (P.C == 0)
			PC = GetBranchOrJmpLocation();
		break;

	case BCS: // Branch on Carry Set
		if (P.C)
			PC = GetBranchOrJmpLocation();
		break;

	case BEQ: // Branch on result zero (equal means compare difference is 0)
		if (P.Z)
			PC = GetBranchOrJmpLocation();
		break;

	case BIT: // Test bits in memory with accumulator
		{
			uint8 result = A & GetMemValue();
			P.N = result & 0x80; // Bit 7
			P.V = result & 0x40; // Bit 6
			P.Z = CalcZeroFlag(result);
		}
		break;

	case BMI: // Branch on result minus
		if (P.N)
			PC = GetBranchOrJmpLocation();
		break;

	case BNE:  // Branch on result non-zero
		if (P.Z == 0)
			PC = GetBranchOrJmpLocation();
		break;

	case BPL: // Branch on result plus
		if (P.N == 0)
			PC = GetBranchOrJmpLocation();
		break;

	case BRK: // Force break (Forced Interrupt PC + 2 toS P toS)
		// NOTE: A BRK command cannot be masked by setting I.
		{
			uint16 returnAddr = PC += m_pEntry->numBytes;
			Push16(returnAddr);
			P.B = 1; // Set break flag before pushing status register
			Push8(P.flags);
			P.I = 1; // Disable hardware IRQs
			PC = m_pRam->Read16(CpuRam::kIrqVector);
		}
		break;

	case BVC: // Branch on Overflow Clear
		if (P.V == 0)
			PC = GetBranchOrJmpLocation();
		break;

	case BVS: // Branch on Overflow Set
		if (P.V)
			PC = GetBranchOrJmpLocation();
		break;

	case CLC: // CLC Clear carry flag
		P.C = 0;
		break;

	case CLD: // CLD Clear decimal mode
		P.D = 0;
		break;

	case CLI: // CLI Clear interrupt disable bit
		P.I = 0;
		break;

	case CLV: // CLV Clear overflow flag
		P.V = 0;
		break;

	case CMP: // CMP Compare memory and accumulator
		// Operation:  A - M                              N Z C I D V
		//                                                / / / _ _ _
		{
			uint16 result = TO16(A) - TO16(GetMemValue());
			P.N = CalcNegativeFlag(result);
			P.Z = CalcZeroFlag(result);
			P.C = CalcCarryFlag(result);
		}
		break;

	case CPX: // CPX Compare Memory and Index X
		{
			uint16 result = TO16(X) - TO16(GetMemValue());
			P.N = CalcNegativeFlag(result);
			P.Z = CalcZeroFlag(result);
			P.C = CalcCarryFlag(result);
		}
		break;

	case CPY: // CPY Compare memory and index Y
		{
			uint16 result = TO16(Y) - TO16(GetMemValue());
			P.N = CalcNegativeFlag(result);
			P.Z = CalcZeroFlag(result);
			P.C = CalcCarryFlag(result);
		}
		break;

	case DEC:
		break;
	case DEX:
		break;
	case DEY:
		break;
	case EOR:
		break;
	case INC:
		break;
	case INX:
		break;
	case INY:
		break;
	case JMP:
		break;
	case JSR:
		break;
	case LDA:
		break;
	case LDX:
		break;
	case LDY:
		break;
	case LSR:
		break;
	case NOP:
		break;
	case ORA:
		break;
	case PHA:
		break;
	case PHP:
		break;
	case PLA:
		break;
	case PLP:
		break;
	case ROL:
		break;
	case ROR:
		break;
	case RTI:
		break;
	case RTS:
		break;
	case SBC:
		break;
	case SEC: // Set carry flag
		P.C = 1;
		break;
	case SED: // Set decimal mode
		P.D = 1;
		break;
	case SEI: // Set interrupt disable status
		P.I = 1;
		break;
	case STA:
		break;
	case STX:
		break;
	case STY:
		break;
	case TAX:
		break;
	case TAY:
		break;
	case TSX:
		break;
	case TXA:
		break;
	case TXS:
		break;
	case TYA:
		break;
	}

	// If instruction hasn't modified PC, move it to next instruction
	if (startPC == PC)
		PC += m_pEntry->numBytes;
}

uint8 Cpu::GetAccumOrMemValue() const
{
	assert(m_pEntry->addrMode == AddressMode::Accumu || m_pEntry->addrMode & AddressMode::MemoryValueOperand);

	if (m_pEntry->addrMode == AddressMode::Accumu)
		return A;
	
	return m_pRam->Read8(m_operandAddress);
}

void Cpu::SetAccumOrMemValue(uint8 value)
{
	assert(m_pEntry->addrMode == AddressMode::Accumu || m_pEntry->addrMode & AddressMode::MemoryValueOperand);

	if (m_pEntry->addrMode == AddressMode::Accumu)
	{
		A = value;
	}
	else
	{
		m_pRam->Write8(m_operandAddress, value);
	}
}

uint8 Cpu::GetMemValue() const
{
	assert(m_pEntry->addrMode & AddressMode::MemoryValueOperand);
	return m_pRam->Read8(m_operandAddress);
}

void Cpu::SetMemValue(uint8 value)
{
	assert(m_pEntry->addrMode & AddressMode::MemoryValueOperand);
	m_pRam->Write8(m_operandAddress, value);
}

uint16 Cpu::GetBranchOrJmpLocation() const
{
	assert(m_pEntry->addrMode & AddressMode::JmpOrBranchOperand);
	return m_operandAddress;
}

void Cpu::Push8(uint8 value)
{
	m_pRam->Write8(CpuRam::kStackBase + SP, value);
	--SP;
}

void Cpu::Push16(uint16 value)
{
	m_pRam->Write16(CpuRam::kStackBase + SP, value);
	SP -= 2;
}

uint8 Cpu::Pop8()
{
	++SP;
	return m_pRam->Read8(CpuRam::kStackBase + SP);
}

uint16 Cpu::Pop16()
{
	SP += 2;
	return m_pRam->Read16(CpuRam::kStackBase + SP);
}

