#pragma once
#include "Base.h"

// Platform-specific system calls

namespace System
{
	void Sleep(uint32 ms);
	bool GetKeyPress(char& key);
	char WaitForKeyPress();
	void DebugBreak();
}
