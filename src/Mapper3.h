#pragma once

#include "Mapper.h"

class Mapper3 : public Mapper
{
public:
	virtual const char* MapperName() const
	{
		return "CNROM";
	}

	virtual void PostInitialize()
	{
		SetChrBankIndex8k(0, 0);
	}

	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value)
	{
		if (cpuAddress >= 0x8000)
		{
			// Switch 8k chr bank at PPU $0000
			const size_t mask = NumChrBanks8k() - 1;
			const size_t bankIndex = value & mask;			
			SetChrBankIndex8k(0, bankIndex);
		}
	}
};
