#include "Rom.h"

void RomHeader::Initialize(uint8 bytes[16])
{
	const uint8 flags6 = bytes[6];
	const uint8 flags7 = bytes[7];

	if (!(bytes[0] == 'N' && bytes[1] == 'E' && bytes[2] == 'S' && bytes[3] == '\x1A'))
		FAIL("Invalid NES header");

	const size_t v = flags7 & BITS(2, 3);
	if (v == BIT(3)) // If only bit 3 is set
		m_nesHeaderType = NesHeaderType::Nes2;
	else if (v == 0 && reinterpret_cast<const uint32&>(bytes[12]) == 0)
		m_nesHeaderType = NesHeaderType::iNes;
	else 
		m_nesHeaderType = NesHeaderType::iNesArchaic;

	m_prgRomBanks = bytes[4];
	m_chrRomBanks = bytes[5];
	if (m_nesHeaderType == NesHeaderType::Nes2)
	{
		m_prgRomBanks |= ((bytes[9] & BITS(0, 1, 2, 3)) << 8);
		m_chrRomBanks |= ((bytes[9] & BITS(4, 5, 6, 7)) << 4);
	}

	m_prgRamBanks = 0;
	switch (m_nesHeaderType)
	{
	case NesHeaderType::iNesArchaic:
		// We have no idea, so assume 1
		m_prgRamBanks = 1;
		break;
	
	case NesHeaderType::iNes:
		m_prgRamBanks = bytes[8];
		// Wiki: Value 0 infers 8 KB for compatibility
		if (m_prgRamBanks == 0)
			m_prgRamBanks = 1;
		break;

	case NesHeaderType::Nes2:
		{
			// BB = Battery-Backed
			const size_t isBBValue = (bytes[10] & BITS(4, 5, 6, 7)) >> 4;
			const size_t noBBValue = (bytes[10] & BITS(0, 1, 2, 3)) >> 0;

			// Values are powers of two, offset by 6 (so 1 is 128, 2 is 256, etc.)
			const size_t isBBSize = 1 << (isBBValue + 6);
			const size_t noBBSize = 1 << (noBBValue + 6);

			m_prgRamBanks = (isBBSize + noBBSize) / KB(8);
		}
		break;
	}

	if (flags6 & BIT(3))
		m_mirroring = NameTableMirroring::FourScreen;
	else
		m_mirroring = (flags6 & BIT(0)) ? NameTableMirroring::Vertical : NameTableMirroring::Horizontal;

	m_mapperNumber = (flags7 & 0xF0) | ((flags6 & 0xF0) >> 4);

	m_hasSaveRam = (flags6 & BIT(1)) != 0;
	m_hasTrainer = (flags6 & BIT(2)) != 0;
	m_isVSUnisystem = (flags7 & BIT(0)) != 0;
	m_isPlayChoice10 = (flags7 & BIT(1)) != 0;
}
