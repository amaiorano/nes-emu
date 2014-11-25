#pragma once

#include "Base.h"
#include <type_traits>

template <typename T>
class Bitfield
{
public:
	static_assert(std::is_unsigned<T>::value, "T must be an unsigned integral");

	Bitfield() { ClearAll(); }

	FORCEINLINE T Value() const { return m_field; }
	FORCEINLINE T SetValue(T value) { return m_field = value; }

	FORCEINLINE void ClearAll() { m_field = 0; }
	FORCEINLINE void SetAll() { m_field = static_cast<T>(~0); }

	FORCEINLINE void Set(T bits, T enabled) { if (enabled) Set(bits); else Clear(bits); }
	FORCEINLINE void Set(T bits) { m_field |= bits; }
	FORCEINLINE void Clear(T bits) { m_field &= ~bits; }
	FORCEINLINE T Read(T bits) const { return m_field & bits; }
	FORCEINLINE bool Test(T bits) const { return Read(bits) != 0; }
	FORCEINLINE T Test01(T bits) const { return Read(bits) != 0? 1 : 0; }

	// Bit position functions
	FORCEINLINE void SetPos(T bitPos, T enabled) { if (enabled) SetPos(bitPos); else ClearPos(bitPos); }
	FORCEINLINE void SetPos(T bitPos) { Set(1 << bitPos); }
	FORCEINLINE void ClearPos(T bitPos) { Clear(1 << bitPos); }
	FORCEINLINE T ReadPos(T bitPos) const { return Read(1 << bitPos); }
	FORCEINLINE bool TestPos(T bitPos) const { return Read(1 << bitPos) != 0; }
	FORCEINLINE T TestPos01(T bitPos) const { return Read(1 << bitPos) != 0? 1 : 0; }

private:
	T m_field;
};

typedef Bitfield<uint8> Bitfield8;
typedef Bitfield<uint16> Bitfield16;
