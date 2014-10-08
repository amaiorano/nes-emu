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
	bool Read(T* pDestBuffer, int count = 1)
	{
		return fread(pDestBuffer, sizeof(T), count, m_pFile) == (sizeof(T) * count);
	}

private:
	FILE* m_pFile;
};
