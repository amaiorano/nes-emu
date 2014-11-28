#include "System.h"

#if PLATFORM_WINDOWS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#undef ARRAYSIZE // Already defined in a Windows header
#include <Windows.h>
#include <conio.h>
#include <cstdio>

// Undef the macro in WinUser.h so we can use this name as our function. We invoke MessageBoxA directly.
#undef MessageBox

namespace System
{
	void Reset() {}
	
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

	void MessageBox(const char* title, const char* message)
	{
		printf(FormattedString<>("%s: %s\n", title, message).Value());
		::MessageBoxA(::GetActiveWindow(), message, title, MB_OK);
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

#elif PLATFORM_MACOSX

#include <unistd.h>
#include <sys/time.h>

namespace System
{
	static timeval firsttime;
	void Reset()
	{
		gettimeofday(&firsttime, NULL);
	}
	
	void Sleep(uint32 ms) { usleep(ms * 1000); }
	
	bool GetKeyPress(char& key)
	{
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
		//::DebugBreak();
	}
	
	void MessageBox(const char* title, const char* message) { }
	
	Ticks GetTicks()
	{
		timeval t;
		gettimeofday(&t, NULL);
		
		return (t.tv_sec - firsttime.tv_sec) * 1000 * 1000 + (t.tv_usec - firsttime.tv_usec);
	}
	
	float64 TicksToSec(Ticks t1)
	{
		return t1 / (1000.0f * 1000.0f);
	}
}

#else
#error "Implement for current platform
#endif
