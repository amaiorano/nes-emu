#include "Cpu.h"
#include "OpCodeTable.h"

#define ADDR_8 "$%02X"
#define ADDR_16 "$%04X"
#define TO16(v8) ((uint16)(v8))
#define TO8(v16) ((uint8)(v16 & 0x00FF))

#define LOG_OP_ENABLED 1

namespace
{
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
		// Check that lower 8 bits are all 0
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

void Cpu::Reset(uint8* pPrgRom, size_t prgRomSize)
{
	PC = A = X = Y = 0;
	SP = 0x0FF; // Starts at top, decremented when pushing
	P.Reset();

	m_ram.LoadPrgRom(pPrgRom, prgRomSize);
}

static void LogOp(uint16 PC, OpCodeEntry* pEntry, const CpuRam& ram)
{
	// Print PC
	printf(ADDR_16 "\t", PC);

	// Print instruction in hex
	for (uint16 i = 0; i < 4; ++i)
	{
		if (i < pEntry->numBytes)
			printf("%02X", ram.Read8(PC + i));
		else
			printf(" ");
	}
	printf("\t");

	// Print opcode name
	printf("%s ", OpCodeName::String[pEntry->opCodeName]);

	// Print operand
	switch (pEntry->addrMode)
	{
	case AddressMode::Immedt:
		{
			const uint8 address = ram.Read8(PC+1);
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
			const int8 offset = ram.Read8(PC+1); // Signed offset in [-128,127]
			const uint16 target = PC + pEntry->numBytes + offset;
			printf(ADDR_8 " ; " ADDR_16 " (%d)", offset, target, offset);
		}
		break;

	case AddressMode::ZeroPg:
		{
			const uint8 address = ram.Read8(PC+1);
			printf(ADDR_8, address);
		}
		break;

	case AddressMode::ZPIdxX:
		{
			const uint8 address = ram.Read8(PC+1);
			printf(ADDR_8 ",X", address);
		}
		break;

	case AddressMode::ZPIdxY:
		{
			const uint8 address = ram.Read8(PC+1);
			printf(ADDR_8 ",Y", address);
		}
		break;

	case AddressMode::Absolu:
		{
			uint16 address = ram.Read16(PC+1);
			printf(ADDR_16, address);
		}
		break;

	case AddressMode::AbIdxX:
		{
			uint16 address = ram.Read16(PC+1);
			printf(ADDR_16 ",X", address);
		}
		break;

	case AddressMode::AbIdxY:
		{
			uint16 address = ram.Read16(PC+1);
			printf(ADDR_16 ",Y", address);
		}
		break;

	case AddressMode::Indrct:
		{
			uint16 address = ram.Read16(PC+1);
			printf("(" ADDR_16 ")", address);
		}
		break;

	case AddressMode::IdxInd:
		{
			const uint8 address = ram.Read8(PC+1);
			printf("(" ADDR_8 ",X)", address);
		}
		break;

	case AddressMode::IndIdx:
		{
			const uint8 address = ram.Read8(PC+1);
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
	// Entry point is located at the Reset interrupt location
	//@TODO: Are we supposed to perform interrupt code here? Like push PC and S, etc.?
	PC = m_ram.Read16(0xFFFC);

	OpCodeEntry** ppOpCodeTable = GetOpCodeTable();

	bool quit = false;
	while (!quit)
	{
		uint8 opCode = m_ram.Read8(PC);
		OpCodeEntry* pEntry = ppOpCodeTable[opCode];

		if (pEntry == nullptr)
		{
			assert(false && "Unknown opcode");
		}

#if LOG_OP_ENABLED
		LogOp(PC, pEntry, m_ram);
#endif

		// For a few operations, the operand is either a memory location or a register (A),
		// so to avoid extra branching, we store a pointer to whichever operand we need.
		uint8* pOperand = nullptr;

		uint8 operand = 0; //@TODO: Remove? Not sure I need this anymore
		uint16 targetPC = 0; // For JMP and conditional branch

		switch (pEntry->addrMode)
		{
		case AddressMode::Immedt:
			operand = m_ram.Read8(PC+1);
			pOperand = m_ram.ReadPtr8(PC+1);
			break;

		case AddressMode::Implid:
			// No operand to output
			pOperand = nullptr;
			break;

		case AddressMode::Accumu:
			pOperand = &A;
			break;

		case AddressMode::Relatv: // For conditional branch instructions
			{
			//@TODO: Don't do this! We should only resolve if the branch condition succeeds.

			// For branch instructions, resolve the target address
			const int8 offset = m_ram.Read8(PC+1); // Signed offset in [-128,127]
			targetPC = PC + pEntry->numBytes + offset;
			}
			break;
		
		case AddressMode::ZeroPg:
			{
			const uint16 address = TO16(m_ram.Read8(PC+1));
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::ZPIdxX:
			{
			const uint16 address = TO16((m_ram.Read8(PC+1) + X)) & 0x00FF; // Wrap around zero-page boundary
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;
		
		case AddressMode::ZPIdxY:
			{
			const uint16 address = TO16((m_ram.Read8(PC+1) + Y)) & 0x00FF; // Wrap around zero-page boundary
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::Absolu:
			{
			uint16 address = m_ram.Read16(PC+1);
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::AbIdxX:
			{
			uint16 address = m_ram.Read16(PC+1) + X;
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::AbIdxY:
			{
			uint16 address = m_ram.Read16(PC+1) + Y;
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::Indrct: // for JMP only
			{
			uint16 address = m_ram.Read16(PC+1);
			targetPC = m_ram.Read16(address);
			}
			break;

		case AddressMode::IdxInd:
			{
			uint16 address = TO16((m_ram.Read8(PC+1) + X)) & 0x00FF; // Get zero page address of bytes holding address of operand
			address = m_ram.Read16(address); // Get operand address
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		case AddressMode::IndIdx:
			{
			uint16 address = TO16(m_ram.Read8(PC+1)); // Get zero page address of bytes holding address of operand
			address = m_ram.Read16(address) + Y; // Get operand address
			operand = m_ram.Read8(address);
			pOperand = m_ram.ReadPtr8(address);
			}
			break;

		default:
			assert(false && "Invalid addressing mode");
			break;
		}

		using namespace OpCodeName;

		switch (pEntry->opCodeName)
		{
		case ADC: // Add memory to accumulator with carry
			{
			// Operation:  A + M + C -> A, C              N Z C I D V
            //                                            / / / _ _ /
			uint16 newA = TO16(A) + TO16(*pOperand) + TO16(P.C);
			P.N = CalcNegativeFlag(newA);
			P.Z = CalcZeroFlag(newA);
			P.C = CalcCarryFlag(newA);
			P.V = CalcOverflowFlag(A, *pOperand, newA);
			A = TO8(newA);
			}
			break;

		case AND: // "AND" memory with accumulator
			{
			// Operation:  A /\ M -> A                    N Z C I D V
            //                                            / / _ _ _ _
			A = A & *pOperand;
			P.N = CalcNegativeFlag(A);
			P.Z = CalcZeroFlag(A);
			}
			break;

		case ASL: // Shift Left One Bit (Memory or Accumulator)
			{
			// Operation:  C <- |7|6|5|4|3|2|1|0| <- 0    N Z C I D V
			//                                            / / / _ _ _
			uint16 temp = TO16(*pOperand) << 1;
			*pOperand = TO8(temp);		
			P.N = CalcNegativeFlag(*pOperand);
			P.Z = CalcZeroFlag(*pOperand);
			P.C = CalcCarryFlag(temp);
			}
			break;

		case BCC:
			break;
		case BCS:
			break;
		case BEQ:
			break;
		case BIT:
			break;
		case BMI:
			break;
		case BNE:
			break;
		case BPL:
			break;
		case BRK:
			break;
		case BVC:
			break;
		case BVS:
			break;
		case CLC:
			break;
		case CLD:
			break;
		case CLI:
			break;
		case CLV:
			break;
		case CMP:
			break;
		case CPX:
			break;
		case CPY:
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

		PC += pEntry->numBytes;
	}
}
