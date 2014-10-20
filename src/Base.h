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

template <typename N> struct CTPrintSize;

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
