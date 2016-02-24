#pragma once

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
	inline void Initialize(Nes&) {}
	inline void Shutdown() {}
	inline void Update() {};
	inline void DumpMemory() {}
	inline void PreCpuInstruction() {}
	inline void PostCpuInstruction() {}
	inline bool IsExecuting() { return false; }
#endif
}
