#pragma once

#include "CpuRam.h"

class Cpu
{
public:
	void Reset(uint8* pPrgRom, size_t prgRomSize);

	void Run();

private:
	CpuRam m_ram;

	// Registers - not using the usual m_ prefix because I find the code looks
	// more straightfoward when using the typical register names
	uint16 PC;	// Program counter
	uint8 SP;	// Stack pointer
	uint8 A;	// Accumulator
	uint8 X;	// X register
	uint8 Y;	// Y register
	struct StatusRegister
	{
		void Reset() { flags = 0; }

		union
		{
			struct
			{
				uint8 N : 1;	// Negative flag (aka Sign flag)
				uint8 V : 1;	// Overflow flag
				uint8 Bit5 : 1;	// Unused
				uint8 B : 1;	// BRK executed (IRQ/software interupt)
				uint8 D : 1;	// Decimal mode
				uint8 I : 1;	// Interrupt enabled
				uint8 Z : 1;	// Zero flag
				uint8 C : 1;	// Carry flag
			};
			uint8 flags;
		};
	} P; // Processor status (flags)
	static_assert(sizeof(StatusRegister)==1, "StatusRegister must be 1 byte");

};
