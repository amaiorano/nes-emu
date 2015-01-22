#pragma once

#include "Base.h"
#include <cstdio>

class FileStream
{
public:
	FileStream() : m_file(nullptr)
	{
	}
	
	~FileStream()
	{
		Close();
	}

	FileStream(const char* name, const char* mode) : m_file(nullptr)
	{
		if (!Open(name, mode))
			FAIL("Failed to open file: %s", name);
	}

	bool Open(const char* name, const char* mode)
	{
		Close();
		m_file = fopen(name, mode);
		return m_file != nullptr;
	}

	void Close()
	{
		if (m_file)
		{
			fclose(m_file);
			m_file = nullptr;
		}
	}

	void SetPos(size_t pos)
	{
		fseek(m_file, pos, 0);
	}

	template <typename T>
	size_t Read(T* destBuffer, int count = 1)
	{
		return fread(destBuffer, sizeof(T), count, m_file) == (sizeof(T) * count);
	}

	template <typename T>
	size_t WriteValue(T value)
	{
		return fwrite(&value, sizeof(T), 1, m_file);
	}

	template <typename T>
	size_t Write(T* srcBuffer, int count = 1)
	{
		//return fread(destBuffer, sizeof(T), count, m_file) == (sizeof(T) * count);
		return fwrite(srcBuffer, sizeof(T), count, m_file);
	}

	void Printf(const char* format, ...);

private:
	FILE* m_file;
};
