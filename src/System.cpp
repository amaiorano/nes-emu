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

	static float64 GetPerfCountTicksPerSec()
	{
		LARGE_INTEGER freq;
		::QueryPerformanceFrequency(&freq);
		return static_cast<float64>(freq.QuadPart);
	}

	Ticks GetTicks()
	{
		LARGE_INTEGER li;
		::QueryPerformanceCounter(&li);
		return li.QuadPart;
	}

	float64 TicksToSec(Ticks t1)
	{
		static float64 ticksPerSec = GetPerfCountTicksPerSec();
		return static_cast<float64>(t1)/ ticksPerSec;
	}
}


#else
#error "Implement for current platform
#endif
