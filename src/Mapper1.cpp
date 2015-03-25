#include "Mapper1.h"
#include "Serializer.h"
#include <algorithm>

void Mapper1::PostInitialize()
{
	m_boardType = DEFAULT;
	if (PrgMemorySize() == KB(512))
		m_boardType = SUROM;

	m_loadReg.Reset();

	m_controlReg.SetValue(BITS(2,3)); // Bits 2,3 of $8000 are set (16k PRG mode, $8000 swappable)
	m_chrReg0.ClearAll();
	m_chrReg1.ClearAll();
	m_prgReg.ClearAll(); // Note WRAM enabled (bit 5 is 0) which earlier MMC1 games sometimes assume

	UpdatePrgBanks();
	UpdateChrBanks();
	UpdateMirroring();
}

void Mapper1::Serialize(class Serializer& serializer)
{
	Base::Serialize(serializer);
	SERIALIZE(m_loadReg);
	SERIALIZE(m_controlReg);
	SERIALIZE(m_chrReg0);
	SERIALIZE(m_chrReg1);
	SERIALIZE(m_prgReg);
	SERIALIZE(m_boardType);
}

void Mapper1::OnCpuWrite(uint16 cpuAddress, uint8 value)
{
	if (cpuAddress < 0x8000)
		return;

	const bool reset = (value & BIT(7)) != 0;

	if (reset)
	{
		m_loadReg.Reset();
		m_controlReg.Set(BITS(2,3)); // Note: other bits unchanged
	}
	else
	{
		const uint8 dataBit = value & BIT(0);
		m_loadReg.SetBit(dataBit);
	
		if (m_loadReg.AllBitsSet())
		{
			switch (cpuAddress & 0xE000)
			{
			case 0x8000:
				m_controlReg.SetValue(m_loadReg.Value());
				UpdatePrgBanks();
				UpdateChrBanks();
				UpdateMirroring();
				break;

			case 0xA000:
				m_chrReg0.SetValue(m_loadReg.Value());
				if (m_boardType == SUROM) // Hijacks CHR reg bit 4 to select PRG 256k bank
				{
					UpdatePrgBanks();
				}
				UpdateChrBanks();
				break;

			case 0xC000:
				m_chrReg1.SetValue(m_loadReg.Value());
				UpdateChrBanks();
				break;

			case 0xE000:
				m_prgReg.SetValue(m_loadReg.Value());
				UpdatePrgBanks();
				break;

			default:
				assert(false);
				break;
			}

			m_loadReg.Reset();
		}
	}
}

void Mapper1::UpdatePrgBanks()
{
	const uint8 bankMode = m_controlReg.Read(BITS(2,3)) >> 2;

	if (bankMode <= 1) // 32k mode
	{
		const size_t mask = NumPrgBanks32k() - 1;
		const uint8 cartBankIndex = (m_prgReg.Read(BITS(0,1,2,3)) >> 1) & mask;

		SetPrgBankIndex32k(0, cartBankIndex); // Ignore low bit in 32k mode
	}
	else // 16k mode
	{
		// There are at most 16 banks of 16K (256K); we take the min here because
		// SUROM returns 32 banks as it has 2 x 256K PRG-ROM chips, but we treat
		// each bank separately (a bit in chrReg0 selects which 256K chip.)
		const size_t mask = std::min<size_t>(16, NumPrgBanks16k()) - 1;
		
		size_t cartBankIndex = m_prgReg.Read(BITS(0,1,2,3)) & mask;
		size_t firstBankIndex = 0;
		size_t lastBankIndex = (NumPrgBanks16k() - 1) & mask;

		// SUROM hijacks bit 4 of chrReg0 to select one of two 256K PRG banks
		// (this applies to the "fixed" banks as well)
		if (m_boardType == SUROM)
		{
			const uint8 prgBankSelect256k = m_chrReg0.Read(BIT(4));
			cartBankIndex |= prgBankSelect256k;
			firstBankIndex |= prgBankSelect256k;
			lastBankIndex |= prgBankSelect256k;
		}

		if (bankMode == 2)
		{
			SetPrgBankIndex16k(0, firstBankIndex);
			SetPrgBankIndex16k(1, cartBankIndex);
		}
		else
		{
			SetPrgBankIndex16k(0, cartBankIndex);
			SetPrgBankIndex16k(1, lastBankIndex);
		}
	}

	const bool bSavRamChipEnabled = m_prgReg.ReadPos(4) == 0;
	SetCanWriteSavMemory(bSavRamChipEnabled); // Technically is chip enable/disable
}

void Mapper1::UpdateChrBanks()
{
	const bool mode8k = m_controlReg.ReadPos(4) == 0; // Otherwise 4k mode
	
	if (mode8k)
	{
		const size_t mask = NumChrBanks8k() - 1;
		SetChrBankIndex8k(0, (m_chrReg0.Value() >> 1) & mask);
	}
	else
	{
		const size_t mask = NumChrBanks4k() - 1;
		SetChrBankIndex4k(0, m_chrReg0.Value() & mask);
		SetChrBankIndex4k(1, m_chrReg1.Value() & mask);
	}
}

void Mapper1::UpdateMirroring()
{
	static NameTableMirroring table[] =
	{
		NameTableMirroring::OneScreenLower,
		NameTableMirroring::OneScreenUpper,
		NameTableMirroring::Vertical,
		NameTableMirroring::Horizontal,
	};

	const uint8 mirroringType = m_controlReg.Read(BITS(0,1));
	SetNameTableMirroring(table[mirroringType]);
}
