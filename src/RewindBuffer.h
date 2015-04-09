#pragma once

#include "Base.h"
#include "Memory.h"
#include "CircularBuffer.h"

class RewindBuffer
{
public:
	RewindBuffer()
		: m_chunkSize(0)
		, m_numChunks(0)
		, m_nextChunkIndex(0)
	{}

	void Initialize(size_t numChunks, size_t chunkSize)
	{
		m_numChunks = numChunks;
		m_chunkSize = chunkSize;
		m_nextChunkIndex = 0;

		m_storage.Initialize(numChunks * chunkSize);
		m_queue.Init(numChunks);
	}

	void Clear()
	{
		m_nextChunkIndex = 0;
		m_queue.Clear();
	}

	size_t GetChunkSize() const
	{
		return m_chunkSize;
	}

	uint8* GetNextChunk()
	{
		if (m_queue.Full())
		{
			size_t dummy;
			m_queue.PopFront(dummy);
		}
		m_queue.PushBack(m_nextChunkIndex);
		uint8* nextChunk = m_storage.RawPtr() + (m_nextChunkIndex * m_chunkSize);
		m_nextChunkIndex = (m_nextChunkIndex + 1) % m_numChunks;
		return nextChunk;
	}

	// Returns nullptr if no used chunks left
	uint8* GetLastUsedChunk()
	{
		size_t lastUsedChunkIndex;
		if (m_queue.PopBack(lastUsedChunkIndex))
		{
			uint8* lastUsedChunk = m_storage.RawPtr() + (lastUsedChunkIndex * m_chunkSize);
			m_nextChunkIndex = (m_nextChunkIndex == 0) ? (m_numChunks - 1) : (m_nextChunkIndex - 1);
			return lastUsedChunk;
		}
		return nullptr;
	}

private:
	size_t m_chunkSize;
	size_t m_numChunks;
	size_t m_nextChunkIndex; // Index of next available chunk in storage
	Memory<DynamicSizeStorage> m_storage;
	CircularBuffer<size_t> m_queue; // Manages queue used chunks (by index)
};
