#pragma once
#include "Base.h"
#include <memory>

class PulseChannel;
class TriangleChannel;
class NoiseChannel;
class FrameCounter;
class AudioDriver;

namespace ApuChannel
{
	enum Type
	{
		Pulse1, Pulse2, Triangle, Noise, NumTypes
	};
}

class Apu
{
public:
	void Initialize();
	void Reset();
	void Execute(uint32 cpuCycles);
	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	
	float32 GetChannelVolume(ApuChannel::Type type) const { return m_channelVolumes[type]; }
	void SetChannelVolume(ApuChannel::Type type, float32 volume);

private:
	friend void DebugDrawAudio(struct SDL_Renderer* renderer);

	bool m_evenFrame;
	float32 m_channelVolumes[ApuChannel::NumTypes];
	std::shared_ptr<FrameCounter> m_frameCounter;
	std::shared_ptr<PulseChannel> m_pulseChannels[2];
	std::shared_ptr<TriangleChannel> m_triangleChannel;
	std::shared_ptr<NoiseChannel> m_noiseChannel;
	float64 m_elapsedCpuCycles;
	std::shared_ptr<AudioDriver> m_audioDriver;
};
