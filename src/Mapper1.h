#pragma once

#include "Mapper.h"
#include "Bitfield.h"

class Mapper1 : public Mapper
{
public:
	typedef Mapper Base;

	virtual const char* MapperName() const
	{
		return "SxROM/MMC1";
	}

	virtual void PostInitialize();
	virtual void Serialize(class Serializer& serializer);
	virtual void OnCpuWrite(uint16 cpuAddress, uint8 value);

private:
	void UpdatePrgBanks();
	void UpdateChrBanks();
	void UpdateMirroring();

	// Special 5 bit register used by mapper
	class LoadRegister
	{
	public:
		LoadRegister() { Reset(); }

		void Reset()
		{
			m_value.ClearAll(); // Not actualy necessary
			m_bitsWritten = 0;
		}

		void SetBit(uint8 bit)
		{
			assert(m_bitsWritten < 5 && "All bits already written, must Reset");
			m_value.SetPos(m_bitsWritten, (bit & 0x01) != 0);
			++m_bitsWritten;
		}

		bool AllBitsSet() const
		{
			return m_bitsWritten == 5;
		}

		uint8 Value() { return m_value.Value(); }

	private:
		Bitfield8 m_value;
		uint8 m_bitsWritten;
	};

	LoadRegister m_loadReg;
	Bitfield8 m_controlReg;
	Bitfield8 m_chrReg0;
	Bitfield8 m_chrReg1;
	Bitfield8 m_prgReg;

	enum BoardType { DEFAULT, SUROM };
	BoardType m_boardType;
};
