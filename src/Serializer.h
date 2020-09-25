#pragma once

#include "Base.h"
#include "Stream.h"
#include <type_traits>
#include <string>

#define SERIALIZE(value) serializer.SerializeValue(#value, value)
#define SERIALIZE_BUFFER(buffer, size) serializer.SerializeBuffer(#buffer, reinterpret_cast<uint8*>(buffer), size)

class Serializer
{
public:
	template <typename SerializableObject>
	static void SaveRootObject(IStream& stream, SerializableObject& serializable)
	{
		Serializer serializer;
		serializer.BeginSave(stream);
		serializer.SerializeObject(serializable);
		serializer.End();
	}

	template <typename SerializableObject>
	static void LoadRootObject(IStream& stream, SerializableObject& serializable)
	{
		Serializer serializer;
		serializer.BeginLoad(stream);
		serializer.SerializeObject(serializable);
		serializer.End();
	}

	void BeginSave(IStream& stream)
	{
		m_saving = true;
		m_stream = &stream; // shared_ptr?
	}

	void BeginLoad(IStream& stream)
	{
		m_saving = false;
		m_stream = &stream;
	}

	void End()
	{
		m_stream->Close();
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
				FAIL("SaveState data mismatch! Looking for %s, found %s", name, nameFromFile.c_str());
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
				FAIL("SaveState data mismatch! Looking for %s, found %s", name, nameFromFile.c_str());
			const size_t sizeRead = ReadBuffer(buffer);
			if (sizeRead != size)
				FAIL("SaveState buffer size mismatch! Expecting %d, got %d", size, sizeRead);
		}
	}

private:
	void WriteString(const std::string& s)
	{
		m_stream->WriteValue<uint32>(s.length());
		m_stream->Write(s.c_str(), s.length());
	}

	size_t ReadString(std::string& s)
	{
		uint32 length;
		size_t bytesRead = m_stream->ReadValue(length);
		if (bytesRead)
		{
			char temp[128];
			m_stream->Read(temp, length);
			temp[length] = '\0';
			s = temp;
		}
		return bytesRead;
	}

	template <typename T>
	void WriteValue(T& value)
	{
		m_stream->WriteValue<uint32>(sizeof(T));
		m_stream->WriteValue(value);
	}

	template <typename T>
	void ReadValue(T& value)
	{
		uint32 size;
		m_stream->ReadValue(size);
		assert(size == sizeof(T));
		m_stream->ReadValue(value);
	}

	void WriteBuffer(uint8* buffer, size_t size)
	{
		m_stream->WriteValue<size_t>(size);
		m_stream->Write(buffer, size);
	}

	size_t ReadBuffer(uint8* buffer)
	{
		size_t size;
		m_stream->ReadValue(size);
		m_stream->Read(buffer, size);
		return size;
	}

	IStream* m_stream;
	bool m_saving;
};
