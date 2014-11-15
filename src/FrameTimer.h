#pragma once

#include "System.h"

class FrameTimer
{
public:
	FrameTimer() { Reset(); }

	void Reset()
	{
		m_lastTime = System::GetTimeSec();
		m_frameTime = 0.0f;
		m_fps = 60.0f;
	}

	void Update(float32 minFrameTime = 0.0f)
	{
		float64 currTime = 0;
		do
		{
			currTime = System::GetTimeSec();
			m_frameTime = static_cast<float32>(currTime - m_lastTime);

		} while (m_frameTime < minFrameTime);

		m_lastTime = currTime;

		m_fps = (m_fps * 0.8f) + (0.2f * (1.0f/(m_frameTime)));
	}

	float64 GetFrameTime() const { return m_frameTime; }
	float64 GetFps() const { return m_fps; }

private:
	float64 m_lastTime;
	float32 m_frameTime;
	float32 m_fps;
};
