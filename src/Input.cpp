#include "Input.h"
#include <SDL_keyboard.h>
#include <SDL_events.h>
#include <memory>

namespace
{
	Uint8 g_currState[SDL_NUM_SCANCODES];
	Uint8 g_lastState[SDL_NUM_SCANCODES];

	void PollEvents()
	{
		// Need to consume all events for window to be responsive
		SDL_Event e;
		while( SDL_PollEvent(&e) )
		{
			if( e.type == SDL_QUIT )
			{
				exit(0);
			}
		}
	}
}

namespace Input
{
	void Update()
	{
		PollEvents();
		memcpy(g_lastState, g_currState, sizeof(g_lastState));
		memcpy(g_currState, SDL_GetKeyboardState(nullptr), sizeof(g_currState));
	}

	bool KeyDown(SDL_Scancode scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_currState[scanCode] != 0;
	}

	bool KeyUp(SDL_Scancode scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_currState[scanCode] == 0;
	}

	bool KeyPressed(SDL_Scancode scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_lastState[scanCode] == 0 && g_currState[scanCode] != 0;
	}

	bool KeyReleased(SDL_Scancode scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_lastState[scanCode] != 0 && g_currState[scanCode] == 0;
	}

	bool AltDown()
	{
		return KeyDown(SDL_SCANCODE_LALT) || KeyDown(SDL_SCANCODE_RALT);
	}

	bool CtrlDown()
	{
		return KeyDown(SDL_SCANCODE_LCTRL) || KeyDown(SDL_SCANCODE_RCTRL);
	}

	bool ShiftDown()
	{
		return KeyDown(SDL_SCANCODE_LSHIFT) || KeyDown(SDL_SCANCODE_RSHIFT);
	}

	const char* GetScancodeName(SDL_Scancode scanCode)
	{
		return SDL_GetScancodeName(scanCode);
	}
}
