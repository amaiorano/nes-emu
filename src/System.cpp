#include "System.h"

#if PLATFORM_WINDOWS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#undef ARRAYSIZE // Already defined in a Windows header
#include <Windows.h>
#include <conio.h>

namespace System
{
	void Sleep(uint32 ms) { ::Sleep(ms); }

	bool GetKeyPress(char& key)
	{
		if (_kbhit())
		{
			int result = _getch();

			// If user presses fn or arrow, we need two _getch() calls
			if (result == 0 || result == 0xE0)
			{
				_getch();
				result = 0xFF; // Return uncommon result
			}

			key = static_cast<char>(result);
			return true;
		}
		return false;
	}
	
	char WaitForKeyPress()
	{
		char key;
		while (!GetKeyPress(key))
		{
			Sleep(1);
		}
		return key;
	}

	void DebugBreak()
	{
		::DebugBreak();
	}
}


#else
#error "Implement for current platform
#endif
