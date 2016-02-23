#include "Apu.h"
#include "Cpu.h"
#include "MemoryBus.h"
#include "AudioDriver.h"
#include "Serializer.h"

#include <Nes_Apu.h>
#include <apu_snapshot.h>

#include <vector>
#include <algorithm>
#include <limits>

#include <SDL_render.h>

static int dmc_read_function(void* memoryReader, cpu_addr_t cpuAddress) {
	auto cpu = reinterpret_cast<Cpu*>(memoryReader);

	cpu->StealCycles(4);
	return cpu->GetMemoryBus()->Read(cpuAddress);
}

Apu::~Apu()
{
}

void Apu::Initialize(Cpu& cpu)
{
	m_audioDriver = std::make_shared<AudioDriver>();
	m_audioDriver->Initialize();

	m_buffer = std::make_shared<Blip_Buffer>();
	m_buffer->clock_rate(1789773);
	m_buffer->sample_rate(m_audioDriver->GetSampleRate(), 500);

	m_apuImpl = std::make_shared<Nes_Apu>();
	m_apuImpl->dmc_reader(dmc_read_function, &cpu);
	m_apuImpl->output(m_buffer.get());

	m_volume = 1.0f;
}

void Apu::Reset()
{
	m_buffer->clear();
	m_apuImpl->reset();//TODO: NTSC only for now
}

void Apu::Serialize(class Serializer& serializer)
{
	apu_snapshot_t apu_snapshot;
	if (serializer.IsSaving())
	{
		m_apuImpl->save_snapshot(&apu_snapshot);
		SERIALIZE(apu_snapshot);
	}
	else {
		SERIALIZE(apu_snapshot);
		m_apuImpl->load_snapshot(apu_snapshot);
	}
}

void Apu::Execute(uint32 currentFrameCycle)
{
	m_apuImpl->run_until(currentFrameCycle);
}

void Apu::EndFrame(uint32 currentFrameCycle) {
	m_buffer->end_frame(currentFrameCycle);
	m_apuImpl->end_frame(currentFrameCycle);

#define MAX_SAMPLES 1024
	blip_sample_t samples[MAX_SAMPLES];
	long numSamples;
	while ((numSamples = m_buffer->read_samples(samples, MAX_SAMPLES)) > 0)
	{
		for (long i = 0; i < numSamples; ++i) {
			float sample = 0.5f + 0.5f * (float)samples[i] / std::numeric_limits<blip_sample_t>::max();
			m_audioDriver->AddSampleF32(sample);
		}
	}
}

uint8 Apu::HandleCpuReadStatus(uint32 currentFrameCycle)
{
	return m_apuImpl->read_status(currentFrameCycle);
}

void Apu::HandleCpuWrite(uint32 currentFrameCycle, uint16 cpuAddress, uint8 value)
{
	m_apuImpl->write_register(currentFrameCycle, cpuAddress, value);
}

float32 Apu::GetChannelVolume(ApuChannel::Type type) const
{
	return m_volume;
}

void Apu::SetChannelVolume(ApuChannel::Type type, float32 volume)
{
	//TODO: we support master volume control only
	m_volume = Clamp(volume, 0.0f, 1.0f);
}


//for debugging
void DebugDrawAudio(struct SDL_Renderer *) {
	//TODO
}