#pragma once
#include "Base.h"
#include <memory>

class FrameCounter;
class PulseChannel;
class TriangleChannel;
class NoiseChannel;
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
	void Serialize(class Serializer& serializer);
	void Execute(uint32 cpuCycles);
	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	
	float32 GetChannelVolume(ApuChannel::Type type) const { return m_channelVolumes[type]; }
	void SetChannelVolume(ApuChannel::Type type, float32 volume);

private:
	float32 SampleChannelsAndMix();
	friend void DebugDrawAudio(struct SDL_Renderer* renderer);
	friend class FrameCounter;

	bool m_evenFrame;
	float64 m_elapsedCpuCycles;
	float32 m_sampleSum;
	float32 m_numSamples;
	float32 m_channelVolumes[ApuChannel::NumTypes];
	std::shared_ptr<FrameCounter> m_frameCounter;
	std::shared_ptr<PulseChannel> m_pulseChannel0;
	std::shared_ptr<PulseChannel> m_pulseChannel1;
	std::shared_ptr<TriangleChannel> m_triangleChannel;
	std::shared_ptr<NoiseChannel> m_noiseChannel;
	std::shared_ptr<AudioDriver> m_audioDriver;
};
