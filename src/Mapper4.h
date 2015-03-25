#pragma once

#include "Mapper.h"

class Mapper4 : public Mapper
{
public:
	typedef Mapper Base;

	virtual const char* MapperName() const
	{
		return "MMC3,MMC6";
	}

	virtual void PostInitialize();
	virtual void Serialize(class Serializer& serializer);
	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value);

	bool TestAndClearIrqPending()
	{
		bool result = m_irqPending;
		m_irqPending = false;
		return result;
	}

	void HACK_OnScanline();

private:
	void UpdateFixedBanks();
	void UpdateBank(uint8 value);

	uint8 m_prgBankMode;
	uint8 m_chrBankMode;
	uint8 m_nextBankToUpdate;

	bool m_irqEnabled;
	uint8 m_irqCounter;
	
	bool m_irqReloadPending;
	uint8 m_irqReloadValue;

	bool m_irqPending;
};
