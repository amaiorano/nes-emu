#pragma once

#include <cstdarg>
#include <cassert>
#include <stdexcept>

#define APP_NAME "nes-emu"

// Platform defines
#ifdef _MSC_VER
	#define PLATFORM_WINDOWS 1
#elif __linux__
	#define PLATFORM_LINUX 1
#elif __APPLE__
	#define PLATFORM_MAC 1
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
	#pragma warning(disable : 4127) // conditional expression is constant	
#endif

#define FORCEINLINE __inline

typedef unsigned char uint8;
typedef char int8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;
typedef int int32;
typedef unsigned long long uint64;
typedef long long int64;
typedef float float32;
typedef double float64;

static_assert(sizeof(uint8)==1, "Invalid type size");
static_assert(sizeof(int8)==1, "Invalid type size");
static_assert(sizeof(uint16)==2, "Invalid type size");
static_assert(sizeof(int16)==2, "Invalid type size");
static_assert(sizeof(uint32)==4, "Invalid type size");
static_assert(sizeof(int32)==4, "Invalid type size");
static_assert(sizeof(uint64)==8, "Invalid type size");
static_assert(sizeof(int64)==8, "Invalid type size");
static_assert(sizeof(float32)==4, "Invalid type size");
static_assert(sizeof(float64)==8, "Invalid type size");

// Use to make a macro value into a string
#define STRINGIZE(v) __STRINGIZE(v)
#define __STRINGIZE(v) #v

#define KB(n) (n*1024)
#define MB(n) (n*1024*1024)

#define TO16(v8) ((uint16)(v8))
#define TO8(v16) ((uint8)(v16 & 0x00FF))

#define ADDR_8 "$%02X"
#define ADDR_16 "$%04X"

#define BIT(n) (1<<n)

// BITS macro evaluates to a size_t with the specified bits set
// Example: BITS(0,2,4) evaluates 10101
namespace Internal
{
	template <size_t value> struct ShiftLeft1 { static const size_t Result = 1 << value; };
	template <> struct ShiftLeft1<~0u> { static const size_t Result = 0; };

	template <
		size_t b0,
		size_t b1 = ~0u,
		size_t b2 = ~0u,
		size_t b3 = ~0u,
		size_t b4 = ~0u,
		size_t b5 = ~0u,
		size_t b6 = ~0u,
		size_t b7 = ~0u,
		size_t b8 = ~0u,
		size_t b9 = ~0u,
		size_t b10 = ~0u,
		size_t b11 = ~0u,
		size_t b12 = ~0u,
		size_t b13 = ~0u,
		size_t b14 = ~0u,
		size_t b15 = ~0u
	>
	struct BitMask
	{
		static const size_t Result =
			ShiftLeft1<b0>::Result |
			ShiftLeft1<b1>::Result |
			ShiftLeft1<b2>::Result |
			ShiftLeft1<b3>::Result |
			ShiftLeft1<b4>::Result |
			ShiftLeft1<b5>::Result |
			ShiftLeft1<b6>::Result |
			ShiftLeft1<b7>::Result |
			ShiftLeft1<b8>::Result |
			ShiftLeft1<b9>::Result |
			ShiftLeft1<b10>::Result |
			ShiftLeft1<b11>::Result |
			ShiftLeft1<b12>::Result |
			ShiftLeft1<b13>::Result |
			ShiftLeft1<b14>::Result |
			ShiftLeft1<b15>::Result;
	};
}
#define BITS(...) Internal::BitMask<__VA_ARGS__>::Result

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

// Little utility to have compiler spit out the size of a type. Declare dummy var of this type,
// and compiler will fail because the type is fully defined, and should spit out the size of your
// type in the error message. Example usage: CTPrintSize<YourType> dummy;
template <size_t N> struct __CTPrintSize;
template <typename T> struct CTPrintSize : __CTPrintSize<sizeof(T)> {};

// Use with std::shared_ptr that manage array allocations.
// Example: std::shared_ptr<uint8> data(new uint8[10], ArrayDeleter<uint8>); 
template <typename T> void ArrayDeleter(T* p) { delete [] p; }

// Utility for creating a temporary formatted string
template <int MaxLength = 1024>
struct FormattedString
{
	FormattedString(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		int result = vsnprintf(buffer, MaxLength, format, args);
		// Safety in case string couldn't find completely: make last character a \0
		if (result < 0 || result >= MaxLength)
		{
			buffer[MaxLength - 1] = 0;
		}
		va_end(args);
	}

	const char* Value() const { return buffer; }

	operator const char*() const { return Value(); }

	char buffer[MaxLength];
};

template <typename T>
T Clamp(T value, T min, T max)
{
	return value < min ? min : value > max ? max : value;
}

// FAIL macro

namespace System { extern void DebugBreak(); }
namespace Debugger { extern void Shutdown(); }

inline void FailHandler(const char* msg)
{
	Debugger::Shutdown(); // Flush buffered output to trace file

#if CONFIG_DEBUG
	printf("FAIL: %s\n", msg);
	System::DebugBreak();
#endif
	throw std::logic_error(msg);
}

// NOTE: Need this helper for clang/gcc so we can pass a single arg to FAIL
#define FAIL_HELPER(msg, ...) FailHandler(FormattedString<>(msg, __VA_ARGS__))
#define FAIL(...) FAIL_HELPER(__VA_ARGS__, "")


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

template <typename T, typename U>
FORCEINLINE bool TestBits(T& target, U value)
{
	return ReadBits(target, value) != 0;
}

template <typename T, typename U>
FORCEINLINE T TestBits01(T& target, U value)
{
	return ReadBits(target, value) != 0? 1 : 0;
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
