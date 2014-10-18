#pragma once
#include "Base.h"

// Platform-specific system calls

namespace System
{
	void Sleep(uint32 ms);
	char WaitForKeyPress();
}
