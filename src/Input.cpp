#include "Input.h"
#include "SDL_keyboard.h"
#include <memory>

namespace
{
	Uint8 g_currState[SDL_NUM_SCANCODES];
	Uint8 g_lastState[SDL_NUM_SCANCODES];
}

namespace Input
{
	void Update()
	{
		memcpy(g_lastState, g_currState, sizeof(g_lastState));
		memcpy(g_currState, SDL_GetKeyboardState(nullptr), sizeof(g_currState));
	}

	bool KeyDown(Uint8 scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_currState[scanCode] != 0;
	}

	bool KeyUp(Uint8 scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_currState[scanCode] == 0;
	}

	bool KeyPressed(Uint8 scanCode)
	{
		if (SDL_GetKeyboardFocus() == nullptr)
			return false;

		return g_lastState[scanCode] == 0 && g_currState[scanCode] != 0;
	}

	bool KeyReleased(Uint8 scanCode)
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
}
