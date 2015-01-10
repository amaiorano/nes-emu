#pragma once

#include "Mapper.h"

class Mapper0 : public Mapper
{
	virtual const char* MapperName() const
	{
		return "NROM";
	}

	virtual void PostInitialize()
	{
		assert(NumPrgBanks16k() == 1 || NumPrgBanks16k() == 2);
		assert(NumChrBanks8k() == 1);
		
		SetPrgBankIndex16k(0, 0);

		if (NumPrgBanks16k() == 1)
		{
			SetPrgBankIndex16k(1, 0); // Both low and high 16k banks are the same
		}
		else
		{
			SetPrgBankIndex16k(1, 1);
		}

		SetChrBankIndex8k(0, 0);
	}

	virtual void OnCpuWrite(uint16 /*cpuAddress*/, uint8 /*value*/)
	{
		// Nothing to do
	}
};

class Mapper2 : public Mapper
{
public:
	virtual const char* MapperName() const
	{
		return "UxROM";
	}

	virtual void PostInitialize()
	{
		assert(NumChrBanks8k() == 0);

		SetCanWriteChrMemory(true);

		// $8000 initialized with first 16K bank
		SetPrgBankIndex16k(0, 0);

		// Last 16K bank is permanently "hard-wired" to $C000 and cannot be swapped
		SetPrgBankIndex16k(1, NumPrgBanks16k() - 1);

		SetChrBankIndex8k(0, 0);
	}

	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value)
	{
		if (cpuAddress >= 0x8000)
		{
			// Switch 16k bank at $8000 to value (2 or 3 bits)
			const size_t mask = NumPrgBanks16k() - 1;
			const size_t bankIndex = value & mask;			
			SetPrgBankIndex16k(0, bankIndex);
		}
	}
};
