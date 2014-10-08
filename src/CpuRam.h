#pragma once
#include "Base.h"
#include <array>

class CpuRam
{
public:
	static const uint16 kPrgRomMaxSize = KB(32);
	static const uint16 kPrgRomLowBase = 0x8000;
	static const uint16 kPrgRomHighBase = 0xC000;
	static const uint16 kStackBase = 0x0100; // Range [$0100,$01FF]

	CpuRam()
	{
		Reset();
	}

	void Reset()
	{
		m_memory.fill(0);
	}

	void LoadPrgRom(uint8* pPrgRom, size_t size)
	{
		switch (size)
		{
		case KB(16):
			memcpy(&m_memory[kPrgRomLowBase], pPrgRom, size);
			memcpy(&m_memory[kPrgRomHighBase], pPrgRom, size);
			break;
		
		case KB(32):
			memcpy(&m_memory[kPrgRomLowBase], pPrgRom, size);
			break;
		
		default:
			throw std::invalid_argument("Invalid PrgRom size");
		}
	}

	uint16 WrapMirroredAddress(uint16 address) const
	{
		if (address < 0x2000) // [$0800,$2000[ mirrors [$0000,$0800[
		{
			return address & ~(0x2000 - 0x800);
		}
		else if (address < 0x4000) // [$2008,$4000[ mirrors [$2000,$2008[
		{
			return address & ~(0x4000 - 0x2008);
		}
		return address;
	}

	uint8 Read8(uint16 address) const
	{
		return m_memory[WrapMirroredAddress(address)];
	}

	uint16 Read16(uint16 address) const
	{
		address = WrapMirroredAddress(address);
		return (m_memory[address + 1] << 8) | m_memory[address];
	}

	// Use with care! Writing more than a single byte can be wrong if crossing a mirror boundary
	uint8* ReadPtr8(uint16 address)
	{
		return &(m_memory[WrapMirroredAddress(address)]);
	}

	// Stack works top down from $01FF to $0100, wraps around with no stack overflow
	void StackPush(uint8& SP, uint8 value)
	{
		m_memory[kStackBase + SP--] = value;
	}

	uint8 StackPop(uint8& SP)
	{
		return m_memory[kStackBase + SP++];
	}

private:
	std::array<uint8, KB(64)> m_memory;
};
