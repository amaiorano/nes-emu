#pragma once

#include "Base.h"
#include <cstdio>
#include <stdexcept>

class FileStream
{
public:
	FileStream() : m_pFile(nullptr)
	{
	}
	
	~FileStream()
	{
		Close();
	}

	FileStream(const char* name, const char* mode) : m_pFile(nullptr)
	{
		if (!Open(name, mode))
			throw std::exception(FormattedString<>("Failed to open file: %s", name));
	}

	bool Open(const char* name, const char* mode)
	{
		Close();
		m_pFile = fopen(name, mode);
		return m_pFile != nullptr;
	}

	void Close()
	{
		if (m_pFile)
		{
			fclose(m_pFile);
			m_pFile = nullptr;
		}
	}

	template <typename T>
	size_t Read(T* pDestBuffer, int count = 1)
	{
		return fread(pDestBuffer, sizeof(T), count, m_pFile) == (sizeof(T) * count);
	}

	template <typename T>
	size_t Write(T value)
	{
		return fwrite(&value, sizeof(T), 1, m_pFile);
	}

	template <typename T>
	size_t Write(T* pBuffer, int count = 1)
	{
		//return fread(pDestBuffer, sizeof(T), count, m_pFile) == (sizeof(T) * count);
		return fwrite(pBuffer, sizeof(T), count, m_pFile);
	}

	void Printf(const char* format, ...);

private:
	FILE* m_pFile;
};
