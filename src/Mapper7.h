#pragma once

#include "Mapper.h"

class Mapper7 : public Mapper
{
public:
	virtual const char* MapperName() const
	{
		return "AxROM";
	}

	virtual void PostInitialize()
	{
		SetPrgBankIndex32k(0, 0);
	}

	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value)
	{
		if (cpuAddress >= 0x8000)
		{
			// Switch 32k chr bank at CPU $8000
			const size_t mask = NumPrgBanks32k() - 1;
			const size_t bankIndex = value & mask;
			SetPrgBankIndex32k(0, bankIndex);

			// Select upper or lower VRAM bank for one-screen mirroring
			const bool selectOneScreenLower = (value & BIT(4)) == 0;
			SetNameTableMirroring(selectOneScreenLower? NameTableMirroring::OneScreenLower : NameTableMirroring::OneScreenUpper);
		}
	}
};
