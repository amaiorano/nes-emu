#pragma once
#include "Base.h"

// Platform-specific system calls

#if PLATFORM_WINDOWS
	#define FILE_FILTER(name, types) name " (" types ")\0" types "\0"
#else
	#define FILE_FILTER(name, types) ""
#endif

namespace System
{
	const char* GetAppDirectory();
	bool CreateDirectory(const char* directory);
	void Sleep(uint32 ms);
	void DebugBreak();
	void MessageBox(const char* title, const char* message);
	bool SupportsOpenFileDialog();
	bool OpenFileDialog(std::string& fileSelected, const char* title = "Open", const char* filter = FILE_FILTER("All files", "*.*"));
	float64 GetTimeSec();
}
