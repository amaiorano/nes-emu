#pragma once

#include "Base.h"

// If set, debugging features are enabled for the emulator (slower)
#define DEBUGGING_ENABLED 0

class Nes;

namespace NesDebugger
{
#if DEBUGGING_ENABLED
	void Initialize(Nes& nes);
	void Shutdown();
	void Update();
	void DumpMemory();
	void PreCpuInstruction();
	void PostCpuInstruction();
	bool IsExecuting();
#else
	FORCEINLINE void Initialize(Nes&) {}
	FORCEINLINE void Shutdown() {}
	FORCEINLINE void Update() {};
	FORCEINLINE void DumpMemory() {}
	FORCEINLINE void PreCpuInstruction() {}
	FORCEINLINE void PostCpuInstruction() {}
	FORCEINLINE bool IsExecuting() { return false; }
#endif
}
