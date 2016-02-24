#pragma once

#include "Base.h"
#include <cstdio>
#include <algorithm>

class IStream
{
public:
	virtual ~IStream()
	{
		// Derived streams should implement destructor and call CloseImpl() directly
	}

	void Close()
	{
		CloseImpl();
	}

	bool IsOpen() const
	{
		return IsOpenImpl();
	}

	void SetPos(size_t pos)
	{
		SetPosImpl(pos);
	}

	template <typename T>
	size_t ReadValue(T& value)
	{
		return ReadImpl(&value, sizeof(T), 1);
	}

	template <typename T>
	size_t Read(T* destBuffer, size_t count = 1)
	{
		return ReadImpl(destBuffer, sizeof(T), count) == (sizeof(T) * count);
	}

	template <typename T>
	size_t WriteValue(const T& value)
	{
		return WriteImpl(&value, sizeof(T), 1);
	}

	template <typename T>
	size_t Write(T* srcBuffer, size_t count = 1)
	{
		return WriteImpl(srcBuffer, sizeof(T), count);
	}

	void Printf(const char* format, ...);

protected:
	virtual void CloseImpl() = 0;
	virtual bool IsOpenImpl() const = 0;
	virtual size_t ReadImpl(void* dest, size_t elemSize, size_t count) = 0;
	virtual size_t WriteImpl(const void* source, size_t elemSize, size_t count) = 0;
	virtual bool SetPosImpl(size_t pos) = 0;
};

// Streams to/from file on disk
class FileStream : public IStream
{
public:
	FileStream() : m_file(nullptr)
	{
	}
	
	FileStream(const char* name, const char* mode) : m_file(nullptr)
	{
		if (!Open(name, mode))
			FAIL("Failed to open file: %s", name);
	}

	virtual ~FileStream()
	{
		CloseImpl();
	}

	bool Open(const char* name, const char* mode)
	{
		Close();
		m_file = fopen(name, mode);
		return m_file != nullptr;
	}

protected:
	virtual void CloseImpl()
	{
		if (m_file)
		{
			fclose(m_file);
			m_file = nullptr;
		}
	}
	
	virtual bool IsOpenImpl() const
	{
		return m_file != nullptr;
	}
	
	virtual size_t ReadImpl(void* dest, size_t elemSize, size_t count)
	{
		return fread(dest, elemSize, count, m_file);
	}
	
	virtual size_t WriteImpl(const void* source, size_t elemSize, size_t count)
	{
		return fwrite(source, elemSize, count, m_file);
	}
	
	virtual bool SetPosImpl(size_t pos)
	{
		return fseek(m_file, pos, 0) == 0;
	}

private:
	FILE* m_file;
};

// Streams to/from a fixed-size block of memory
class MemoryStream : public IStream
{
public:
	void Open(uint8* buffer, size_t size)
	{
		m_buffer = buffer;
		m_curr = m_buffer;
		m_size = size;
	}

protected:
	uint8* End() { return m_curr + m_size; }

	virtual void CloseImpl()
	{
		m_curr = nullptr;
		// We leave buffer alone in case it gets reused. Memory will be reclaimed when stream is destroyed.
	}
	
	virtual bool IsOpenImpl() const
	{
		return m_curr != nullptr;
	}

	virtual size_t ReadImpl(void* dest, size_t elemSize, size_t count)
	{
		//@TODO: instead of asserting, read what we can and return amount read
		const size_t size = elemSize * count;
		assert(m_curr + size <= End());
		std::copy_n(m_curr, size, (uint8*)dest);
		m_curr += size;
		return size;
	}

	virtual size_t WriteImpl(const void* source, size_t elemSize, size_t count)
	{
		//@TODO: instead of asserting, write what we can and return amount written
		const size_t size = elemSize * count;
		assert(m_curr + size <= End());
		std::copy_n((uint8*)source, size, m_curr);
		m_curr += size;
		return size;
	}

	virtual bool SetPosImpl(size_t pos)
	{
		//@TODO: instead of asserting, return false if we can't set pos
		assert(pos < m_size);
		m_curr = m_buffer + pos;
		return true;
	}

private:
	uint8* m_buffer;
	uint8* m_curr;
	size_t m_size;
};

// Stream that counts the number of bytes that would be written
class ByteCounterStream : public IStream
{
public:
	ByteCounterStream() : m_size(0) {}

	size_t GetStreamSize() const { return m_size; }

protected:
	virtual void CloseImpl() {}

	virtual bool IsOpenImpl() const { return true; }

	virtual size_t ReadImpl(void* /*dest*/, size_t /*elemSize*/, size_t /*count*/)
	{
		assert(false); // For couting output streams only
		return 1;
	}

	virtual size_t WriteImpl(const void* /*source*/, size_t elemSize, size_t count)
	{
		m_size += (elemSize * count);
		return (elemSize * count);
	}

	virtual bool SetPosImpl(size_t /*pos*/)
	{
		assert(false); // Not supported
		return false;
	}

private:
	size_t m_size;
};
