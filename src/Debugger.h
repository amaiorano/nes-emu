#pragma once

#include "Base.h"

// If set, debugging features are enabled for the emulator (slower)
#define DEBUGGING_ENABLED 1

class Nes;

namespace Debugger
{
#if DEBUGGING_ENABLED
	void Initialize(Nes& nes);
	void Update();
	void PreCpuInstruction();
	void PostCpuInstruction();
#else
	FORCEINLINE void Initialize(Nes&) {}
	FORCEINLINE void Update() {}
	FORCEINLINE void PreCpuInstruction() {}
	FORCEINLINE void PostCpuInstruction() {}
#endif
}
