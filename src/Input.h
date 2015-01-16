#pragma once
#include "Base.h"
#include "SDL_scancode.h"

namespace Input
{
	// Call once per frame
	void Update();

	bool KeyDown(Uint8 scanCode);
	bool KeyUp(Uint8 scanCode);

	bool KeyPressed(Uint8 scanCode);
	bool KeyReleased(Uint8 scanCode);

	bool AltDown();
	bool CtrlDown();
	bool ShiftDown();
}
