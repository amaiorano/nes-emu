#pragma once

#include "Base.h"
#include "FileStream.h"
#include <type_traits>

#define SERIALIZE(value) serializer.SerializeValue(#value, value)
#define SERIALIZE_BUFFER(buffer, size) serializer.SerializeBuffer(#buffer, reinterpret_cast<uint8*>(buffer), size)

class Serializer
{
public:
	bool BeginSave(const char* file)
	{
		m_saving = true;
		return m_fs.Open(file, "wb");
	}

	bool BeginLoad(const char* file)
	{
		m_saving = false;
		return m_fs.Open(file, "rb");
	}

	void End()
	{
		m_fs.Close();
	}

	// Client is expected to implement a function with signature:
	//   void Serialize(class Serializer& serializer, bool saving);
	template <typename SerializableObject>
	void SerializeObject(SerializableObject& serializable)
	{
		serializable.Serialize(*this);
	}

	// Use SERIALIZE macro to invoke this function
	template <typename T>
	void SerializeValue(const char* name, T& value)
	{
		// This catches types with vtables, non-trivial copy constructors and assignment operators, etc.
		// But we can't determine if the type aggregates a pointer, which would be a problem.
		static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable to serialize");
		static_assert(!std::is_pointer<T>::value, "Unsafe to serialize a pointer");

		if (m_saving)
		{
			WriteString(name);
			WriteValue(value);
		}
		else
		{
			std::string nameFromFile;
			ReadString(nameFromFile);
			if (nameFromFile.compare(name) != 0)
				FAIL("SaveState data mismatch! Looking for %s, found %s", name, nameFromFile);
			ReadValue(value);
		}
	}

	// User SERIALIZE_BUFFER macro to invoke this function
	void SerializeBuffer(const char* name, uint8* buffer, size_t size)
	{
		if (m_saving)
		{
			WriteString(name);
			WriteBuffer(buffer, size);
		}
		else
		{
			std::string nameFromFile;
			ReadString(nameFromFile);
			if (nameFromFile.compare(name) != 0)
				FAIL("SaveState data mismatch! Looking for %s, found %s", name, nameFromFile);
			const size_t sizeRead = ReadBuffer(buffer);
			if (sizeRead != size)
				FAIL("SaveState buffer size mismatch! Expecting %d, got %d", size, sizeRead);
		}
	}

private:
	void WriteString(const std::string& s)
	{
		m_fs.WriteValue<uint32>(s.length());
		m_fs.Write(s.c_str(), s.length());
	}

	size_t ReadString(std::string& s)
	{
		uint32 length;
		size_t bytesRead = m_fs.ReadValue(length);
		if (bytesRead)
		{
			char temp[128];
			m_fs.Read(temp, length);
			temp[length] = '\0';
			s = temp;
		}
		return bytesRead;
	}

	template <typename T>
	void WriteValue(T& value)
	{
		m_fs.WriteValue<uint32>(sizeof(T));
		m_fs.WriteValue(value);
	}

	template <typename T>
	void ReadValue(T& value)
	{
		uint32 size;
		m_fs.ReadValue(size);
		assert(size == sizeof(T));
		m_fs.ReadValue(value);
	}

	void WriteBuffer(uint8* buffer, size_t size)
	{
		m_fs.WriteValue<size_t>(size);
		m_fs.Write(buffer, size);
	}

	size_t ReadBuffer(uint8* buffer)
	{
		size_t size;
		m_fs.ReadValue(size);
		m_fs.Read(buffer, size);
		return size;
	}

	FileStream m_fs;
	bool m_saving;
};
