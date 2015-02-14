#pragma once

#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>

template <typename T>
class CircularBuffer
{
public:
	void Init(size_t maxSize)
	{
		m_buffer.resize(maxSize);
		m_read = m_write = &m_buffer.front();
		m_end = &m_buffer.back() + 1;
		m_wrapped = 0;
	}

	// Attempts to write numValues from source into buffer; will not go past the read pointer
	void Write(T* source, size_t numValues)
	{
		assert(m_wrapped < 2);

		if (m_wrapped == 0) // read is behind write
		{
			const size_t roomLeft = m_end - m_write;
			const size_t numValuesForFirstWrite = std::min(numValues, roomLeft);

			std::copy_n(source, numValuesForFirstWrite, m_write);
			m_write += numValuesForFirstWrite;
			assert(m_write <= m_end);

			if (m_write == m_end)
			{
				m_write = &m_buffer.front();
				++m_wrapped;
			}

			// If we've written all we have to, bail; otherwise, we still have more to write, so
			// update our inputs and fall through into m_wrapped == 1 condition below
			if (numValuesForFirstWrite == numValues)
			{
				return;
			}
			else
			{
				source += numValuesForFirstWrite;
				numValues -= numValuesForFirstWrite;
			}
		}

		// Note: NOT "else if" here on purpose
		if (m_wrapped == 1) // read is ahead of write
		{
			// Write as much as we can; but we can't go past the read pointer
			size_t roomLeft = m_read - m_write;
			const size_t numValuesToWrite = std::min(numValues, roomLeft);
			std::copy_n(source, numValuesToWrite, m_write);
			m_write += numValuesToWrite;
			assert(m_write <= m_read);
		}
	}

	// Attempts to read numValues worth of data from the buffer into dest, returns how many values actually read
	size_t Read(T* dest, size_t numValues)
	{
		assert(m_wrapped < 2);
		size_t numValuesActuallyRead = 0;

		if (m_wrapped == 1) // read is ahead of write
		{
			const size_t roomLeft = m_end - m_read;
			const size_t numValuesToRead = std::min(numValues, roomLeft);

			std::copy_n(m_read, numValuesToRead, dest);
			numValuesActuallyRead += numValuesToRead;
			m_read += numValuesToRead;
			assert(m_read <= m_end);

			if (m_read == m_end)
			{
				m_read = &m_buffer.front();
				--m_wrapped;
			}

			// If we've read all we have to, bail; otherwise, we still have more to read, so
			// update our inputs and fall through into m_wrapped == 0 condition below
			if (numValuesToRead == numValues)
			{
				return numValuesActuallyRead;
			}
			else
			{
				dest += numValuesToRead;
				numValues -= numValuesToRead;
			}
		}

		// Note: NOT "else if" here on purpose
		if (m_wrapped == 0) // read is behind write
		{
			// Read as much as we can; but we can't go past the write pointer
			size_t roomLeft = m_write - m_read;
			const size_t numValuesToRead = std::min(numValues, roomLeft);
			std::copy_n(m_read, numValuesToRead, dest);
			numValuesActuallyRead += numValuesToRead;
			m_read += numValuesToRead;
			assert(m_read <= m_write);
		}

		return numValuesActuallyRead;
	}

	//@TODO: Optimize for single value write
	void Write(T value)
	{
		Write(&value, 1);
	}

	//@TODO: Optimize for single value read
	T Read()
	{
		T value;
		Read(&value, 1);
		return value;
	}

private:
	std::vector<T> m_buffer;
	T* m_read;
	T* m_write;
	T* m_end;
	int m_wrapped;
};
