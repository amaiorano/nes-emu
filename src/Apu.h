#pragma once
#include "Base.h"
#include <memory>

class Cpu;
class FrameCounter;
class PulseChannel;
class TriangleChannel;
class NoiseChannel;
class AudioDriver;

class Nes_Apu;
class Blip_Buffer;

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
	~Apu();

	void Initialize(Cpu& cpu);
	void Reset();
	void Serialize(class Serializer& serializer);
	void Execute(uint32 currentFrameCycle);
	void EndFrame(uint32 currentFrameCycle);
	uint8 HandleCpuReadStatus(uint32 currentFrameCycle);
	void HandleCpuWrite(uint32 currentFrameCycle, uint16 cpuAddress, uint8 value);
	
	float32 GetChannelVolume(ApuChannel::Type type) const;
	void SetChannelVolume(ApuChannel::Type type, float32 volume);

private:
	std::shared_ptr<Nes_Apu> m_apuImpl;
	std::shared_ptr<Blip_Buffer> m_buffer;
	std::shared_ptr<AudioDriver> m_audioDriver;

	float32 m_volume;
};
