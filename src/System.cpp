#include "System.h"
#include "IO.h"
#include <SDL_filesystem.h>
#include <string>
#include <string.h>
#include <thread>

namespace System
{
	const char* GetAppDirectory()
	{
		static char appDir[2048] = { 0 };
		
		// Lazily build the app directory
		if (appDir[0] == 0)
		{
			std::string temp = SDL_GetBasePath();

			// Find the app name directory in the path and return full path to that directory.
			// Mostly useful when running out of Debug/Release during development.
			auto npos = temp.find(APP_NAME);
			if (npos != std::string::npos)
			{
				temp.copy(appDir, npos + strlen(APP_NAME) + 1, 0);
			}
			else
			{
				temp.copy(appDir, temp.length(), 0);
			}

			temp = appDir; // Just for asserting
			assert(temp.size() > 0 && temp.size() < ARRAYSIZE(appDir));
			assert(temp.back() == '\\' || temp.back() == '/');
		}

		return appDir;
	}
}

#if PLATFORM_WINDOWS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#undef ARRAYSIZE // Already defined in a Windows header
#include <Windows.h>
#include <commdlg.h>
#include <conio.h>
#include <cstdio>

// Undef the macros in WinUser.h so we can use these names
#undef MessageBox
#undef CreateDirectory

namespace System
{
	bool CreateDirectory(const char* directory)
	{
		return ::CreateDirectoryA(directory, NULL) != FALSE;
	}

	void Sleep(uint32 ms)
	{
		::Sleep(ms);
	}

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

	bool OpenFileDialog(std::string& fileSelected, const char* title, const char* filter)
	{
		char file[_MAX_PATH] = "";

		char currDir[_MAX_PATH] = "";
		::GetCurrentDirectoryA(sizeof(currDir), currDir);

		OPENFILENAMEA ofn = {0};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = file;
		ofn.nMaxFile = sizeof(file);
		ofn.lpstrTitle = title;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 0;
		ofn.lpstrInitialDir = currDir;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (::GetOpenFileNameA(&ofn) == TRUE)
		{
			fileSelected = ofn.lpstrFile;
			return true;
		}
		return false;
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
#elif PLATFORM_MAC
	
#include <sys/stat.h> // mkdir
#include <curses.h>
#include <stdexcept>

#include <mach/mach_time.h>

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
	
namespace System
{
	bool CreateDirectory(const char* directory)
	{
		return mkdir(directory, 0755) != -1;
	}
	
	void Sleep(uint32 ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
	
	bool GetKeyPress(char& key)
	{
		throw std::runtime_error("GetKeyPress is unimplemented");
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
		//TODO
	}
	
	void MessageBox(const char* title, const char* message)
	{
		NSAlert * alert = [[NSAlert alloc] init];
		alert.messageText = [NSString stringWithUTF8String: title];
		alert.informativeText = [NSString stringWithUTF8String: message];
		
		[alert addButtonWithTitle: @"OK"];
		
		[alert runModal];
		
		[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
	}
	
	bool OpenFileDialog(std::string& fileSelected, const char* title, const char* filter)
	{
		char* filter_copy = strdup(filter);
		if (!filter_copy)
			return false;
		NSMutableArray* nsFilterTypes = [NSMutableArray array];
		char* tokCtxt;
		char* types = strtok_r(filter_copy, ", ", &tokCtxt);
		while (types != NULL)
		{
			NSString* filterType = [NSString stringWithUTF8String:types];
			NSRange dot = [filterType rangeOfString:@"." options:NSBackwardsSearch];
			NSString* ext = [filterType substringFromIndex:dot.location + 1];
			
			[nsFilterTypes addObject:ext];
			types = strtok_r(NULL, ", ", &tokCtxt);
		}
		
		free(filter_copy);
		
		NSOpenPanel* panel = [NSOpenPanel openPanel];
		
		panel.title = [NSString stringWithUTF8String:title];
		
		panel.canChooseFiles = YES;
		panel.allowedFileTypes = nsFilterTypes;
		
		NSUInteger res = [panel runModal];
		[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
		
		if ( res == NSModalResponseOK && [panel.URLs count] > 0) {
			NSURL* url = [panel.URLs objectAtIndex:0];
			
			fileSelected = [url.path UTF8String];
			
			return true;
		}
		return false;
	}
	
	Ticks GetTicks()
	{
		//avoid time overflow
		static Ticks startTick = mach_absolute_time();
		return mach_absolute_time() - startTick;
	}
	
	float64 TicksToSec(Ticks t1)
	{
		static mach_timebase_info_data_t timebase = {0};
		if (timebase.denom == 0)
			mach_timebase_info(&timebase);
		
		return static_cast<float64>(t1) * (double)timebase.numer /
			(double) timebase.denom / 1e9;
	}
}
#else
#error "Implement for current platform
#endif
