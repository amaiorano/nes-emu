#pragma once
#include "Base.h"

// Platform-specific system calls

#if PLATFORM_WINDOWS
	#define FILE_FILTER(name, types) name " (" types ")\0" types "\0"
#endif

namespace System
{
	void Sleep(uint32 ms);
	bool GetKeyPress(char& key);
	char WaitForKeyPress();
	void DebugBreak();
	void MessageBox(const char* title, const char* message);
	bool OpenFileDialog(std::string& fileSelected, const char* title = "Open", const char* filter = FILE_FILTER("All files", "*.*"));

	typedef uint64 Ticks;
	Ticks GetTicks();
	float64 TicksToSec(Ticks t1);
	inline float64 GetTimeSec() { return TicksToSec(GetTicks()); }
}
