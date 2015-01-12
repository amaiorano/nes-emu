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
