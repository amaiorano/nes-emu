#include "Apu.h"
#include "Nes.h"
#include "Memory.h"
#include "Bitfield.h"
#include "MemoryMap.h"
#include "Debugger.h"
#include <tuple>
#include "SDL_audio.h"

void Apu::StaticSDLAudioCallback(void *userdata, uint8 *stream, int len)
{
	((Apu*)userdata)->SDLAudioCallback(stream, len);
}

Apu::Apu()
{
	OpenSDLAudio();
	GenerateWaveForm();
	Reset();
	//m_debugFileDump.Open("audiodump-S16LSB.raw", "wb");
}

Apu::~Apu()
{
	m_debugFileDump.Close();
}

void Apu::OpenSDLAudio()
{
	SDL_AudioSpec desired, obtained;

	desired.freq = 44100;

	/* 16-bit signed audio */
	desired.format = AUDIO_S16LSB;

	desired.channels = 1;

	/* Large audio buffer reduces risk of dropouts but increases response time */
	// Use a power-of-two buffer length closest to 60hz (one frame) but smaller: 44100 / 60 = 735 so will will use 512
	desired.samples = 512;

	desired.callback = StaticSDLAudioCallback;
	desired.userdata = this;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, &obtained) < 0)
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		exit(-1);
	}
	m_lastPause = false;
}

void Apu::SetSDLAudioPause()
{
	SDL_PauseAudio(m_lastPause);
}

void Apu::GenerateWaveForm()
{
	int16 amplitude = 700;
	for (int j = 0; j < 4; j++)
	{
		for (int i = 0; i < 8; i++)
		{
			m_pulseWaveFormDuty[j][i] = -amplitude;
		}
	}
	m_pulseWaveFormDuty[0][1] = amplitude;

	m_pulseWaveFormDuty[1][1] = amplitude;
	m_pulseWaveFormDuty[1][2] = amplitude;

	m_pulseWaveFormDuty[2][1] = amplitude;
	m_pulseWaveFormDuty[2][2] = amplitude;
	m_pulseWaveFormDuty[2][3] = amplitude;
	m_pulseWaveFormDuty[2][4] = amplitude;

	m_pulseWaveFormDuty[3][0] = amplitude;
	m_pulseWaveFormDuty[3][3] = amplitude;
	m_pulseWaveFormDuty[3][4] = amplitude;
	m_pulseWaveFormDuty[3][5] = amplitude;
	m_pulseWaveFormDuty[3][6] = amplitude;
	m_pulseWaveFormDuty[3][7] = amplitude;

	for (int i = 0; i < 16; i++)
	{
		m_triangleWaveForm[i + 16] = (uint16) (i * 2 * amplitude / 16 - amplitude);
		m_triangleWaveForm[-i + 15] = m_triangleWaveForm[i + 16];
	}

	m_noiseFreq[0] = 4;
	m_noiseFreq[1] = 8;
	m_noiseFreq[2] = 16;
	m_noiseFreq[3] = 32;
	m_noiseFreq[4] = 64;
	m_noiseFreq[5] = 96;
	m_noiseFreq[6] = 128;
	m_noiseFreq[7] = 160;
	m_noiseFreq[8] = 202;
	m_noiseFreq[9] = 254;
	m_noiseFreq[10] = 380;
	m_noiseFreq[11] = 508;
	m_noiseFreq[12] = 762;
	m_noiseFreq[13] = 1016;
	m_noiseFreq[14] = 2034;
	m_noiseFreq[15] = 4068;

	for (int i = 0; i < 32768; i++)
	{
		m_noiseWaveForm[i] = rand() % 511 - 256;
	}
}

void Apu::Reset() {
	m_frameCounter = 0;
	for (int i = 0; i < 2; i++)
	{
		m_pulses[i].m_pulseTimer = 0;
		m_pulses[i].m_pulseVolume = 0;
		m_pulses[i].m_pulseSequencer = 0;
	}
	m_triangleSequencer = 0;
	m_triangleTimer = 0;
	m_noiseSequencer = 0;
	SetSDLAudioPause();
}

void Apu::SDLAudioCallback(uint8 *stream, int len)
{
	// Convert parameters to 16-bit since we are using "Signed 16-bit" audio
	int16* buffer = (int16*)stream;
	len >>= 1;
	FillSoundBuffer(buffer, len);
}

void Apu::FillSoundBuffer(int16 *buffer, int len)
{
	memset(buffer, 0, len << 1);
	// Mix in both pulse (rectangle / square waves)
	for (int p = 0; p < 2; p++)
	{
		int freq = 1789773 / (16 * (m_pulses[p].m_pulseTimer + 1));

		//printf("frame %i, SDLAudioCallback len=%i\n", len);
		int volume = 0;
		if (m_pulses[p].m_pulseConstant)
		{
			volume = m_pulses[p].m_pulseVolume;
		}
		if (m_pulses[p].m_pulseTimer < 8)
		{
			volume = 0;
		}

		for (int i = 0; i < len; i++) {
			//buffer[i] = (int16)(sin((float)(i + m_frameCounter * len) / 44100 * M_PI * 2.f * freq) * volume);
			uint8 sequencer = (m_pulses[p].m_pulseSequencer / 5512) & 7;
			m_pulses[p].m_pulseSequencer += freq;
			buffer[i] += (int16) (m_pulseWaveFormDuty[m_pulses[p].m_pulseDuty][sequencer] * volume);
		}
	}

	{
		// Mix in triangle wave (bass)
		int freq = 1789773 / (32 * (m_triangleTimer + 1));
		int volume = 10;
		for (int i = 0; i < len; i++) {
			uint8 sequencer = (m_triangleSequencer / 1378) & 31;
			m_triangleSequencer += freq;
			buffer[i] += (int16)(m_triangleWaveForm[sequencer] * volume);
		}
	}

	{
		// Mix in noise wave (drum and effects)
		int freq = 1789773 / (16 * (m_noiseFreq[m_noisePeriod] + 1));
		int volume = m_noiseVolume;
		for (int i = 0; i < len; i++) {
			uint16 sequencer = (m_noiseSequencer / 1378) & 32767;
			m_noiseSequencer += freq;
			buffer[i] += (int16)(m_triangleWaveForm[sequencer] * volume);
		}
	}

	if (m_debugFileDump.IsOpen())
	{
		m_debugFileDump.Write(buffer, len);
	}
	++m_frameCounter;
}

uint8 Apu::HandleCpuRead(uint16 cpuAddress)
{
	//printf("frame %i, HandleCpuRead, adress=%04X\n", frameCounter, cpuAddress);
	return 0;
}

void Apu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	//printf("frame %i, HandleCpuWrite, adress=%04X = %i\n", frameCounter, cpuAddress, value);
	for (int p = 0; p < 2; p++)
	{
		uint16 pulseAddressOffset = uint16(p << 2);
		if (cpuAddress == CpuMemory::kApuPulse1ChannelA + pulseAddressOffset)
		{
			m_pulses[p].m_pulseDuty = (value & BITS(7, 6)) >> 6;
			m_pulses[p].m_pulseLoop = (value & BITS(5)) >> 5;
			m_pulses[p].m_pulseConstant = (value & BITS(4)) >> 4;
			m_pulses[p].m_pulseVolume = (value & BITS(0, 1, 2, 3)) >> 0;
		}
		else if (cpuAddress == CpuMemory::kApuPulse1ChannelB + pulseAddressOffset)
		{
			m_pulses[p].m_pulseSweepEnabled = (value & BITS(7)) >> 7;
			m_pulses[p].m_pulseSweepPeriod = (value & BITS(6, 5, 4)) >> 4;
			m_pulses[p].m_pulseSweepNeg = (value & BITS(3)) >> 3;
			m_pulses[p].m_pulseSweepShift = (value & BITS(0, 1, 2)) >> 0;
		}
		else if (cpuAddress == CpuMemory::kApuPulse1ChannelC + pulseAddressOffset)
		{
			m_pulses[p].m_pulseTimer = (m_pulses[p].m_pulseTimer & ~0xFF) | value;
		}
		else if (cpuAddress == CpuMemory::kApuPulse1ChannelD + pulseAddressOffset)
		{
			m_pulses[p].m_pulseLength = (value & BITS(7, 6, 5, 4, 3)) >> 3;
			m_pulses[p].m_pulseTimer = (m_pulses[p].m_pulseTimer & ~0x700) | ((value & BITS(0, 1, 2)) << 8);
		}
	}

	switch (cpuAddress)
	{
	case CpuMemory::kApuTriangleChannelA:
		m_triangleControlFlag = (value & BITS(7)) >> 7;
		m_triangleCounterReload = (value & ~BITS(7)) >> 0;
		break;
	case CpuMemory::kApuTriangleChannelB:
		m_triangleTimer = (m_triangleTimer & ~0xFF) | value;
		break;
	case CpuMemory::kApuTriangleChannelC:
		m_triangleLength = (value & BITS(7, 6, 5, 4, 3)) >> 3;
		m_triangleTimer = (m_triangleTimer & ~0x700) | ((value & BITS(0, 1, 2)) << 8);
		break;
	}

	switch (cpuAddress)
	{
	case CpuMemory::kApuNoiseChannelA:
		m_noiseHaltFlag = (value & BITS(5)) >> 5;
		m_noiseConstant = (value & BITS(4)) >> 4;
		m_noiseVolume = (value & BITS(0,1,2,3)) >> 0;
		break;
	case CpuMemory::kApuNoiseChannelB:
		m_noiseMode = (value & BITS(7)) >> 7;
		m_noisePeriod = (value & BITS(0, 1, 2, 3)) >> 0;
		break;
	case CpuMemory::kApuNoiseChannelC:
		m_noiseLength = (value & BITS(7, 6, 5, 4, 3)) >> 3;
		break;
	}
}

void Apu::OutputFrame(bool paused)
{
	if (m_lastPause != paused)
	{
		m_lastPause = paused;
		SetSDLAudioPause();
	}
	if (paused) return;
}
