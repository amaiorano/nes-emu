#pragma once

#include "Base.h"
#include <array>
#include <vector>

template <size_t size>
class FixedSizeStorage
{
public:
	static const size_t kSize = size;

	void Initialize()
	{
		m_memory.fill(0);
	}

	size_t Size() const { return kSize; }

protected:
	std::array<uint8, size> m_memory;
};

class DynamicSizeStorage
{
public:
	void Initialize(size_t size)
	{
		m_memory.resize(size);
		std::fill(begin(m_memory), end(m_memory), 0);
	}

	size_t Size() { return m_memory.size(); }

protected:
	std::vector<uint8> m_memory;
};

template <typename StorageType>
class Memory : public StorageType
{
public:
	using StorageType::m_memory;
	using StorageType::Size;
	
	uint8 Read(uint16 address)
	{
		return m_memory[address];
	}

	void Write(uint16 address, uint8 value)
	{
		m_memory[address] = value;
	}

	uint8* RawPtr(uint16 address = 0)
	{
		return &m_memory[address];
	}

	uint8& RawRef(uint16 address = 0)
	{
		return m_memory[address];
	}

	template <typename T>
	T RawPtrAs(uint16 address = 0)
	{
		return reinterpret_cast<T>(&m_memory[address]);
	}

	const uint8* Begin() const { return &m_memory[0]; }
	const uint8* End() const { return Begin() + Size(); }
};
