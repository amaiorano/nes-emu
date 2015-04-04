#pragma once

#include "Base.h"
#include <memory>

class Nes;
class RewindBuffer;

const size_t kRewindSaveStateFrameInterval = 1;
const float64 kRewindLoadStateTimeInterval = (1 / 60.0) * kRewindSaveStateFrameInterval;
const float64 kRewindMaxTime = 60.0;
const size_t kRewindNumSaveStates = static_cast<size_t>((60.0 / kRewindSaveStateFrameInterval) * kRewindMaxTime);

class RewindManager
{
public:
	RewindManager();
	
	void Initialize(Nes& nes);	
	void ClearRewindStates();
	
	void SetRewinding(bool enable);	
	bool IsRewinding() const { return m_rewinding; 	}
	
	void SaveRewindState();

	// Returns true if a frame was rewinded (based on timer interval)
	bool RewindFrame();

private:
	Nes* m_nes;
	bool m_rewinding;
	std::shared_ptr<RewindBuffer> m_rewindBufferHolder;
	RewindBuffer* m_rewindBuffer;
	size_t m_rewindFrameCount;
	float64 m_lastRewindTime;
};
