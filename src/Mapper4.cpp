#include "Mapper4.h"
#include "Serializer.h"

// http://wiki.nesdev.com/w/index.php/INES_Mapper_004

void Mapper4::PostInitialize()
{
	// Last virtual bank ($E000-$FFFF) is always fixed to last physical bank
	SetPrgBankIndex8k(3, NumPrgBanks8k() - 1);

	m_irqEnabled = false;
	m_irqReloadPending = false;
	m_irqPending = false;
}

void Mapper4::Serialize(class Serializer& serializer)
{
	Base::Serialize(serializer);
	SERIALIZE(m_prgBankMode);
	SERIALIZE(m_chrBankMode);
	SERIALIZE(m_nextBankToUpdate);
	SERIALIZE(m_irqEnabled);
	SERIALIZE(m_irqCounter);
	SERIALIZE(m_irqReloadPending);
	SERIALIZE(m_irqReloadValue);
	SERIALIZE(m_irqPending);
}

void Mapper4::OnCpuWrite(uint16 cpuAddress, uint8 value)
{
	const uint16 mask = BITS(15,14,13,0); // Top 3 bits for register, low bit for high/low part of register

	cpuAddress &= mask;

	switch (cpuAddress)
	{
	case 0x8000:
		m_chrBankMode = (value & BIT(7)) >> 7;
		m_prgBankMode = (value & BIT(6)) >> 6;
		m_nextBankToUpdate = value & BITS(0,1,2);
		UpdateFixedBanks();
		break;

	case 0x8001:
		UpdateBank(value);
		break;

	case 0xA000:
		SetNameTableMirroring((value & BIT(0)) == 0? NameTableMirroring::Vertical : NameTableMirroring::Horizontal);
		break;

	case 0xA001:
		{
			// [EW.. ....]
			// E = Enable WRAM (0=disabled, 1=enabled)
			// W = WRAM write protect (0=writable, 1=not writable)
			// As long as WRAM is enabled and non write-protected, we can write to it
			const bool canWriteSavRam = ((value & BITS(7,6)) == BIT(7));
			SetCanWriteSavMemory(canWriteSavRam);
		}
		break;

	case 0xC000:
		// Value copied to counter when counter == 0 OR reload is pending
		// (at next rising edge)
		m_irqReloadValue = value;
		break;

	case 0xC001:
		m_irqReloadPending = true;
		break;

	case 0xE000:
		m_irqEnabled = false;
		m_irqPending = false;
		break;

	case 0xE001:
		m_irqEnabled = true;
		break;
	};
}

void Mapper4::UpdateFixedBanks()
{
	// Update the fixed second-to-last bank
	SetPrgBankIndex8k((1 - m_prgBankMode) * 2, NumPrgBanks8k() - 2);
}

void Mapper4::UpdateBank(uint8 value)
{
	const size_t chrBankMask1k = NumChrBanks1k() - 1;
	const size_t prgBankMask8k = NumPrgBanks8k() - 1;

	switch (m_nextBankToUpdate)
	{
	// value is 1K CHR bank index
	case 0: 
		SetChrBankIndex1k(m_chrBankMode * 4 + 0, (value & 0xFE) & chrBankMask1k);
		SetChrBankIndex1k(m_chrBankMode * 4 + 1, (value | 0x01) & chrBankMask1k);
		break;
	case 1:
		SetChrBankIndex1k(m_chrBankMode * 4 + 2, (value & 0xFE) & chrBankMask1k);
		SetChrBankIndex1k(m_chrBankMode * 4 + 3, (value | 0x01) & chrBankMask1k);
		break;
	case 2: SetChrBankIndex1k((1 - m_chrBankMode) * 4 + 0, value & chrBankMask1k); break;
	case 3: SetChrBankIndex1k((1 - m_chrBankMode) * 4 + 1, value & chrBankMask1k); break;
	case 4: SetChrBankIndex1k((1 - m_chrBankMode) * 4 + 2, value & chrBankMask1k); break;
	case 5: SetChrBankIndex1k((1 - m_chrBankMode) * 4 + 3, value & chrBankMask1k); break;

	// value is 8K PRG bank index
	case 6: SetPrgBankIndex8k(m_prgBankMode * 2 + 0, value & prgBankMask8k); break;
	case 7: SetPrgBankIndex8k(1, value & prgBankMask8k); break;
	}
}

void Mapper4::HACK_OnScanline()
{
	if (m_irqCounter == 0 || m_irqReloadPending)
	{
		m_irqCounter = m_irqReloadValue;
		m_irqReloadPending = false;
	}
	else
	{
		--m_irqCounter;
		if (m_irqCounter == 0 && m_irqEnabled)
		{
			// Trigger IRQ - for now set the flag...
			m_irqPending = true;
		}
	}
}
