#include "RewindManager.h"
#include "RewindBuffer.h"
#include "Serializer.h"
#include "System.h"
#include "Nes.h"

RewindManager::RewindManager()
	: m_nes(nullptr)
{
}

void RewindManager::Initialize(Nes& nes)
{
	m_nes = &nes;
	m_rewinding = false;

	m_rewindBufferHolder = std::make_shared<RewindBuffer>();
	m_rewindBuffer = m_rewindBufferHolder.get();

	// Determine size of save state for currently loaded rom
	ByteCounterStream bcs;
	Serializer::SaveRootObject(bcs, *m_nes);

	m_rewindBuffer->Initialize(kRewindNumSaveStates, bcs.GetStreamSize());
	m_rewindFrameCount = 0;
}

void RewindManager::ClearRewindStates()
{
	m_rewindBuffer->Clear();
}

void RewindManager::SetRewinding(bool enable)
{
	if (!m_rewinding && enable)
	{
		m_lastRewindTime = System::GetTimeSec();
	}

	m_rewinding = enable;
}

void RewindManager::SaveRewindState()
{
	if (++m_rewindFrameCount == kRewindSaveStateFrameInterval)
	{
		m_rewindFrameCount = 0;
		MemoryStream ms;
		ms.Open(m_rewindBuffer->GetNextChunk(), m_rewindBuffer->GetChunkSize());
		Serializer::SaveRootObject(ms, *m_nes);
	}
}

// Returns true if a frame was rewinded (based on timer interval)
bool RewindManager::RewindFrame()
{
	const float64 currTime = System::GetTimeSec();
	if (currTime - m_lastRewindTime >= kRewindLoadStateTimeInterval)
	{
		if (uint8* lastUsedChunk = m_rewindBuffer->GetLastUsedChunk())
		{
			MemoryStream ms;
			ms.Open(lastUsedChunk, m_rewindBuffer->GetChunkSize());
			m_nes->Reset();
			Serializer::LoadRootObject(ms, *m_nes);

			m_lastRewindTime = currTime;

			return true;
		}
	}
	return false;
}
