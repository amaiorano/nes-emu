#pragma once
#include "Memory.h"
#include "MemoryMap.h"
#include "Serializer.h"

class CpuInternalRam
{
public:
	void Initialize()										{ m_memory.Initialize(); }

	void Serialize(class Serializer& serializer)
	{
		SERIALIZE(m_memory);
	}

	uint8 HandleCpuRead(uint16 cpuAddress)					{ return m_memory.Read(MapCpuToInternalRam(cpuAddress)); }
	void HandleCpuWrite(uint16 cpuAddress, uint8 value)		{ m_memory.Write(MapCpuToInternalRam(cpuAddress), value); }

private:
	uint16 MapCpuToInternalRam(uint16 cpuAddress)
	{
		assert(cpuAddress < CpuMemory::kInternalRamEnd);
		return cpuAddress % CpuMemory::kInternalRamSize;
	}

	Memory<FixedSizeStorage<KB(2)>> m_memory;
};
