#pragma once

#include <cstdarg>
#include <cassert>

// Platform defines
#ifdef _MSC_VER
	#define PLATFORM_WINDOWS 1
#else
	#error "Define current platform"
#endif

// Build config defines
#if defined(_DEBUG)
	#define CONFIG_DEBUG 1
#endif

// Disable warnings
#if PLATFORM_WINDOWS
	#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

typedef unsigned char uint8;
typedef char int8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;
typedef int int32;
static_assert(sizeof(uint8)==1, "Invalid type size");
static_assert(sizeof(int8)==1, "Invalid type size");
static_assert(sizeof(uint16)==2, "Invalid type size");
static_assert(sizeof(int16)==2, "Invalid type size");
static_assert(sizeof(uint32)==4, "Invalid type size");
static_assert(sizeof(int32)==4, "Invalid type size");

#define KB(n) (n*1024)
#define MB(n) (n*1024*1024)

#define TO16(v8) ((uint16)(v8))
#define TO8(v16) ((uint8)(v16 & 0x00FF))

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define FORCEINLINE __inline

// Little utility to have compiler spit out the size of a type. Declare dummy var of this type,
// and compiler will fail because the type is fully defined, and should spit out the size of your
// type in the error message. Example usage: CTPrintSize<YourType> dummy;
template <size_t N> struct __CTPrintSize;
template <typename T> struct CTPrintSize : __CTPrintSize<sizeof(T)> {};

// Use with std::shared_ptr that manage array allocations.
// Example: std::shared_ptr<uint8> pData(new uint8[10], ArrayDeleter<uint8>); 
template <typename T> void ArrayDeleter(T* p) { delete [] p; }

template <int MaxLength = 1024>
struct FormattedString
{
	FormattedString(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, MaxLength, format, args);
		va_end(args);
	}

	const char* Value() const { return buffer; }

	operator const char*() const { return Value(); }

	char buffer[MaxLength];
};

// Bit operations

template <typename T, typename U>
FORCEINLINE void SetBits(T& target, U value)
{
	target |= value;
}

template <typename T, typename U>
FORCEINLINE void ClearBits(T& target, U value)
{
	target &= ~value;
}

template <typename T, typename U>
FORCEINLINE T ReadBits(T& target, U value)
{
	return target & value;
}

// Metafunction that returns position of single bit in bit flag
// (in fact, returns the position of the most significant bit, or 0)
template <size_t Value>
struct BitFlagToPos
{
	static const size_t Result = 1 + BitFlagToPos<(Value >> 1)>::Result;
};

template <>
struct BitFlagToPos<0>
{
	static const size_t Result = 0;
};
