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
	
	char WaitForKeyPress()
	{
		while (!_kbhit())
		{
			Sleep(1);
		}

		int result = _getch();

		// If user presses fn or arrow, we need two _getch() calls
		if (result == 0 || result == 0xE0)
		{
			_getch();
			result = 0xFF; // Return uncommon result
		}
		
		return static_cast<char>(result);
	}
}


#else
#error "Implement for current platform
#endif
