#include "Apu.h"
#include "AudioDriver.h"
#include "Bitfield.h"
#include <vector>
#include <algorithm>

// Temp for debug drawing
#include "Renderer.h"
#include <SDL_render.h>

#define APU_TO_CPU_CYCLE(cpuCycle) static_cast<size_t>(cpuCycle * 2)

Apu* g_apu = nullptr; //@HACK: get rid of this


// Divider outputs a clock periodically.
// Note that the term 'period' in this code really means 'period reload value', P,
// where the actual output clock period is P + 1.
class Divider
{
public:
	Divider() : m_period(0), m_counter(0) {}

	size_t GetPeriod() const { return m_period; }
	size_t GetCounter() const { return m_counter;  }

	void SetPeriod(size_t period)
	{
		m_period = period;
	}

	void ResetCounter()
	{
		m_counter = m_period;
	}

	bool Clock()
	{
		// We count down from P to 0 inclusive, clocking out every P + 1 input clocks.
		if (m_counter-- == 0)
		{
			ResetCounter();
			return true;
		}
		return false;
	}

private:
	size_t m_period;
	size_t m_counter;
};

// When LengthCounter reaches 0, corresponding channel is silenced
// http://wiki.nesdev.com/w/index.php/APU_Length_Counter
class LengthCounter
{
public:
	LengthCounter() : m_enabled(false), m_halt(false), m_counter(0) {}

	void SetEnabled(bool enabled)
	{
		m_enabled = enabled;
		
		// Disabling resets counter to 0, and it stays that way until enabled again
		if (!m_enabled)
			m_counter = 0;
	}

	void SetHalt(bool halt)
	{
		m_halt = halt;
	}

	void LoadCounterFromLUT(uint8 index)
	{
		if (!m_enabled)
			return;

		static uint8 lut[] = 
		{ 
			10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
			12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
		};
		static_assert(ARRAYSIZE(lut) == 32, "Invalid");
		assert(index < ARRAYSIZE(lut));

		m_counter = lut[index];
	}

	// Clocked by FrameCounter
	void Clock()
	{
		if (m_halt) // Halting locks counter at current value
			return;

		if (m_counter > 0) // Once it reaches 0, it stops, and channel is silenced
			--m_counter;
	}

	size_t GetValue() const
	{
		return m_counter;
	}

	bool SilenceChannel() const
	{
		return m_counter == 0;
	}

private:
	bool m_enabled;
	bool m_halt;
	size_t m_counter;
};

// Controls volume in 2 ways: decreasing saw with optional looping, or constant volume
// Input: Clocked by Frame Sequencer
// Output: 4-bit volume value (0-15)
// http://wiki.nesdev.com/w/index.php/APU_Envelope
class VolumeEnvelope
{
public:
	VolumeEnvelope()
		: m_restart(true)
		, m_loop(false)
		, m_counter(0)
		, m_constantVolumeMode(false)
		, m_constantVolume(0)
	{
	}

	void Restart() { m_restart = true; }
	void SetLoop(bool loop) { m_loop = loop;  }
	void SetConstantVolumeMode(bool mode) { m_constantVolumeMode = mode; }

	void SetConstantVolume(uint16 value)
	{
		assert(value < 16);
		m_constantVolume = value;
		m_divider.SetPeriod(m_constantVolume); // Constant volume doubles up as divider reload value
	}

	size_t GetVolume() const
	{
		size_t result = m_constantVolumeMode ? m_constantVolume : m_counter;
		assert(result < 16);
		return result;
	}

	// Clocked by FrameCounter
	void Clock()
	{
		if (m_restart)
		{
			m_restart = false;
			m_counter = 15;
			m_divider.ResetCounter();
		}
		else
		{
			if (m_divider.Clock())
			{
				if (m_counter > 0)
				{
					--m_counter;
				}
				else if (m_loop)
				{
					m_counter = 15;
				}
			}
		}
	}
private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	bool m_restart;
	bool m_loop;
	Divider m_divider;
	size_t m_counter; // Saw envelope volume value (if not constant volume mode)
	bool m_constantVolumeMode;
	size_t m_constantVolume; // Also reload value for divider
};

class AudioChip
{
public:
	virtual void Clock() = 0;
};

// Produces the square wave based on one of 4 duty cycles
// http://wiki.nesdev.com/w/index.php/APU_Pulse
class PulseWaveGenerator : public AudioChip
{
public:
	PulseWaveGenerator() : m_duty(0), m_step(0) {}

	void Restart()
	{
		m_step = 0;
	}

	void SetDuty(uint8 duty)
	{
		assert(duty < 4);
		m_duty = duty;
	}

	// Clocked by an Timer, outputs bit (0 or 1)
	virtual void Clock()
	{
		m_step = (m_step + 1) % 8;
	}

	size_t GetValue() const
	{
		static uint8 sequences[4][8] =
		{
			{ 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
			{ 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
			{ 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
			{ 1, 0, 0, 1, 1, 1, 1, 1 }  // 25% negated
		};

		const uint8 value = sequences[m_duty][m_step];
		return value;
	}

private:
	uint8 m_duty; // 2 bits
	uint8 m_step; // 0-7
};

// A timer is used in each of the five channels to control the sound frequency. It contains a divider which is
// clocked by the CPU clock. The triangle channel's timer is clocked on every CPU cycle, but the pulse, noise,
// and DMC timers are clocked only on every second CPU cycle and thus produce only even periods.
// http://wiki.nesdev.com/w/index.php/APU_Misc#Glossary
class Timer
{
public:
	Timer() : m_outputChip(nullptr), m_minPeriod(0) {}

	void Connect(AudioChip& outputChip)
	{
		assert(m_outputChip == nullptr);
		m_outputChip = &outputChip;
	}

	void Reset()
	{
		m_divider.ResetCounter();
	}

	size_t GetPeriod() const { return m_divider.GetPeriod(); }

	void SetPeriod(size_t period)
	{
		m_divider.SetPeriod(period);
	}

	void SetPeriodLow8(uint8 value)
	{
		size_t period = m_divider.GetPeriod();
		period = (period & BITS(8,9,10)) | value; // Keep high 3 bits
		SetPeriod(period);
	}

	void SetPeriodHigh3(size_t value)
	{
		assert(value < BIT(3));
		size_t period = m_divider.GetPeriod();
		period = (value << 8) | (period & 0xFF); // Keep low 8 bits
		m_divider.SetPeriod(period);

		m_divider.ResetCounter();
	}

	void SetMinPeriod(size_t minPeriod)
	{
		m_minPeriod = minPeriod;
	}

	// Clocked by CPU clock every cycle (triangle channel) or second cycle (pulse/noise channels)
	void Clock()
	{
		// Avoid popping and weird noises from ultra sonic frequencies
		if (m_divider.GetPeriod() < m_minPeriod)
			return;

		if (m_divider.Clock())
		{
			m_outputChip->Clock();
		}
	}

private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	Divider m_divider;
	AudioChip* m_outputChip;
	size_t m_minPeriod;
};

// Periodically adjusts the period of the Timer, sweeping the frequency high or low over time
// http://wiki.nesdev.com/w/index.php/APU_Sweep
class SweepUnit
{
public:
	SweepUnit()
		: m_subtractExtra(0)
		, m_enabled(false)
		, m_negate(false)
		, m_reload(false)
		, m_silenceChannel(false)
		, m_shiftCount(0)
		, m_timer(nullptr)
		, m_targetPeriod(0)
	{
	}

	void SetSubtractExtra()
	{
		m_subtractExtra = 1;
	}

	void Connect(Timer& timer)
	{
		assert(m_timer == nullptr);
		m_timer = &timer;
	}

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	void SetNegate(bool negate) { m_negate = negate; }

	void SetPeriod(size_t period)
	{
		assert(period < 8); // 3 bits
		m_divider.SetPeriod(period); // Don't reset counter
		ComputeTargetPeriod();
	}

	void SetShiftCount(uint8 shiftCount)
	{
		assert(shiftCount < BIT(3));
		m_shiftCount = shiftCount;
	}

	void Restart() { m_reload = true;  }

	// Clocked by FrameCounter
	void Clock()
	{
		ComputeTargetPeriod();

		if (m_reload)
		{
			// From nesdev wiki: "If the divider's counter was zero before the reload and the sweep is enabled,
			// the pulse's period is also adjusted". What this effectively means is: if the divider would have
			// clocked and reset as usual, adjust the timer period.
			if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod();
			}

			m_divider.ResetCounter();

			m_reload = false;
		}
		else
		{
#if 0
			// Only clock divider while sweep is enabled
			if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod();
			}
#else
			// From the nesdev wiki, it looks like the divider is always decremented, but only
			// reset to its period if the sweep is enabled.
			if (m_divider.GetCounter() > 0)
			{
				m_divider.Clock();
			}
			else if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod();
			}
#endif
		}
	}

	bool SilenceChannel() const
	{
		return m_silenceChannel;
	}

private:
	void ComputeTargetPeriod()
	{
		assert(m_shiftCount < 8); // 3 bits

		const size_t currPeriod = m_timer->GetPeriod();
		const size_t shiftedPeriod = currPeriod >> m_shiftCount;

		if (m_negate)
		{
			// Pulse 1's adder's carry is hardwired, so the subtraction adds the one's complement
			// instead of the expected two's complement (as pulse 2 does)
			m_targetPeriod = currPeriod - (shiftedPeriod - m_subtractExtra);
		}
		else
		{
			m_targetPeriod = currPeriod + shiftedPeriod;
		}

		// Channel will be silenced under certain conditions even if Sweep unit is disabled
		m_silenceChannel = (currPeriod < 8 || m_targetPeriod > 0x7FF);
	}

	void AdjustTimerPeriod()
	{
		// If channel is not silenced, it means we're in range
		if (m_enabled && m_shiftCount > 0 && !m_silenceChannel)
		{
			m_timer->SetPeriod(m_targetPeriod);
		}
	}

private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	size_t m_subtractExtra;
	bool m_enabled;
	bool m_negate;
	bool m_reload;
	bool m_silenceChannel; // This is the Sweep -> Gate connection, if true channel is silenced
	uint8 m_shiftCount; // [0,7]
	Divider m_divider;
	Timer* m_timer;
	size_t m_targetPeriod; // Target period for the timer; is computed continuously in real hardware
};

// http://wiki.nesdev.com/w/index.php/APU_Pulse
class PulseChannel
{
public:
	PulseChannel()
	{
		// Connect timer to sequencer
		m_timer.Connect(m_pulseWaveGenerator);

		// Connect sweep unit to timer
		m_sweepUnit.Connect(m_timer);
	}

	size_t GetValue() const
	{
		//@TODO: Maybe we should use gates

		if (m_sweepUnit.SilenceChannel())
			return 0;

		if (m_lengthCounter.SilenceChannel())
			return 0;

		auto value = m_volumeEnvelope.GetVolume() * m_pulseWaveGenerator.GetValue();

		assert(value < 16);
		return value;
	}

//private:
	VolumeEnvelope m_volumeEnvelope;
	SweepUnit m_sweepUnit;
	Timer m_timer;
	PulseWaveGenerator m_pulseWaveGenerator;
	LengthCounter m_lengthCounter;
};

// A counter used by TriangleChannel clocked twice as often as the LengthCounter.
// Is called "linear" because it is fed the period directly rather than an index
// into a look up table like the LengthCounter.
class LinearCounter
{
public:
	LinearCounter() : m_reload(true), m_control(true) {}

	void Restart() { m_reload = true; }

	// If control is false, counter will keep reloading to input period.
	// One way to disable Triangle channel is to set control to false and
	// period to 0 (via $4008), and then restart the LinearCounter (via $400B)
	void SetControlAndPeriod(bool control, size_t period)
	{
		m_control = control;
		assert(period < BIT(7));
		m_divider.SetPeriod(period);
	}
	
	// Clocked by FrameCounter every CPU cycle
	void Clock()
	{
		if (m_reload)
		{
			m_divider.ResetCounter();
		}
		else if (m_divider.GetCounter() > 0)
		{
			m_divider.Clock();
		}

		if (!m_control)
		{
			m_reload = false;
		}
	}

	// If zero, sequencer is not clocked
	size_t GetValue() const
	{
		return m_divider.GetCounter();
	}

	bool SilenceChannel() const
	{
		return GetValue() == 0;
	}

private:
	bool m_reload;
	bool m_control;
	Divider m_divider;
};

class TriangleWaveGenerator : public AudioChip
{
public:
	TriangleWaveGenerator() : m_step(0) {}

	virtual void Clock()
	{
		m_step = (m_step + 1) % 32;
	}

	size_t GetValue() const
	{
		static size_t sequence[] =
		{
			15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		};
		assert(m_step < 32);
		return sequence[m_step];
	}

private:
	uint8 m_step;
};

// Proxy that handles clocking TriangleWaveGenerator when both linear and length counters are non-zero
class TriangleWaveGeneratorClocker : public AudioChip
{
public:
	TriangleWaveGeneratorClocker()
		: m_linearCounter(nullptr)
		, m_lengthCounter(nullptr)
		, m_triangleWaveGenerator(nullptr)
	{
	}

	virtual void Clock()
	{
		if (m_linearCounter->GetValue() > 0 && m_lengthCounter->GetValue() > 0)
			m_triangleWaveGenerator->Clock();
	}

	LinearCounter* m_linearCounter;
	LengthCounter* m_lengthCounter;
	TriangleWaveGenerator* m_triangleWaveGenerator;
};

class TriangleChannel
{
public:
	TriangleChannel()
	{
		m_timer.Connect(m_clocker);
		m_timer.SetMinPeriod(2); // Avoid popping from ultrasonic frequencies

		//@TODO: Connect funcs
		m_clocker.m_linearCounter = &m_linearCounter;
		m_clocker.m_lengthCounter = &m_lengthCounter;
		m_clocker.m_triangleWaveGenerator = &m_triangleWaveGenerator;
	}

	size_t GetValue() const
	{
		//@NOTE: From nesdev wiki main APU page:
		// "Silencing the triangle channel merely halts it. It will continue to output its last value, rather than 0."
		// So we don't return 0 if channel is silenced.
	#if 0
		if (m_linearCounter.SilenceChannel())
			return 0;

		if (m_lengthCounter.SilenceChannel())
			return 0;
	#endif

		return m_triangleWaveGenerator.GetValue();
	}

	LinearCounter m_linearCounter;
	LengthCounter m_lengthCounter;
	Timer m_timer;
	TriangleWaveGeneratorClocker m_clocker;
	TriangleWaveGenerator m_triangleWaveGenerator;
};

class LinearFeedbackShiftRegister : public AudioChip
{
public:
	LinearFeedbackShiftRegister() : m_register(1), m_mode(false){}

	virtual void Clock()
	{
		uint16 bit0 = ReadBits(m_register, BIT(0));
		uint16 bitN = m_mode ? ReadBits(m_register, BIT(6)) >> 6 : ReadBits(m_register, BIT(1)) >> 1;
		uint16 feedback = bit0 ^ bitN;
		m_register = (m_register >> 1) | (feedback << 14);
		assert(m_register < BIT(15));
	}

	bool SilenceChannel() const
	{
		// If bit 0 is set, silence
		return TestBits(m_register, BIT(0));
	}

	uint16 m_register;
	bool m_mode;
};

class NoiseChannel
{
public:
	NoiseChannel()
	{
		m_volumeEnvelope.SetLoop(true); // Always looping
		m_timer.Connect(m_shiftRegister);
	}

	size_t GetValue() const
	{
		if (m_shiftRegister.SilenceChannel() || m_lengthCounter.SilenceChannel())
			return 0;

		return m_volumeEnvelope.GetVolume();
	}

//private:
	VolumeEnvelope m_volumeEnvelope;
	Timer m_timer;
	LengthCounter m_lengthCounter;
	LinearFeedbackShiftRegister m_shiftRegister;
};

// aka Frame Sequencer
// http://wiki.nesdev.com/w/index.php/APU_Frame_Counter
class FrameCounter
{
public:
	FrameCounter()
		: m_cpuCycles(0)
		, m_numSteps(4)
		, m_inhibitInterrupt(true)
	{
	}

	void AddConnection(VolumeEnvelope& e) { m_envelopes.push_back(&e); }
	void AddConnection(LengthCounter& lc) { m_lengthCounters.push_back(&lc); }
	void AddConnection(SweepUnit& su) { m_sweepUnits.push_back(&su); }
	void AddConnection(LinearCounter& lc) { m_linearCounters.push_back(&lc); }

	void SetMode(uint8 mode)
	{
		assert(mode < 2);
		if (mode == 0)
		{
			m_numSteps = 4;
		}
		else
		{
			m_numSteps = 5;
			ClockQuarterFrameChips();
			ClockHalfFrameChips();
		}

		// Always restart sequence
		m_cpuCycles = 0;
	}

	void AllowInterrupt() { m_inhibitInterrupt = false; }

	// Clock every CPU cycle
	void Clock()
	{
		bool resetCycles = false;

		switch (m_cpuCycles)
		{
		case APU_TO_CPU_CYCLE(3728.5):
			ClockQuarterFrameChips();
			break;

		case APU_TO_CPU_CYCLE(7456.5):
			ClockQuarterFrameChips();
			ClockHalfFrameChips();
			break;

		case APU_TO_CPU_CYCLE(11185.5):
			ClockQuarterFrameChips();
			break;

		case APU_TO_CPU_CYCLE(14914):
			if (m_numSteps == 4)
			{
				//@TODO: set interrupt flag if !inhibit
			}
			break;

		case APU_TO_CPU_CYCLE(14914.5):
			if (m_numSteps == 4)
			{
				//@TODO: set interrupt flag if !inhibit
				ClockQuarterFrameChips();
				ClockHalfFrameChips();
			}
			break;

		case APU_TO_CPU_CYCLE(14915):
			if (m_numSteps == 4)
			{
				//@TODO: set interrupt flag if !inhibit

				resetCycles = true;
			}
			break;

		case APU_TO_CPU_CYCLE(18640.5):
			assert(m_numSteps == 5);
			{
				ClockQuarterFrameChips();
				ClockHalfFrameChips();
			}
			break;

		case APU_TO_CPU_CYCLE(18641):
			assert(m_numSteps == 5);
			{
				resetCycles = true;
			}
			break;
		}

		m_cpuCycles = resetCycles ? 0 : m_cpuCycles + 1;
	}

private:
	template <typename Container>
	void ClockAll(Container& container)
	{
		for (auto& curr : container)
		{
			curr->Clock();
		}
	}

	void ClockQuarterFrameChips()
	{
		ClockAll(m_envelopes);
		ClockAll(m_linearCounters);
	}

	void ClockHalfFrameChips()
	{
		ClockAll(m_lengthCounters);
		ClockAll(m_sweepUnits);
	}

	size_t m_cpuCycles;
	size_t m_numSteps;
	bool m_inhibitInterrupt;
	std::vector<VolumeEnvelope*> m_envelopes;
	std::vector<LengthCounter*> m_lengthCounters;
	std::vector<SweepUnit*> m_sweepUnits;
	std::vector<LinearCounter*> m_linearCounters;
};

void Apu::Initialize()
{
	g_apu = this;

	m_evenFrame = true;

	std::fill(std::begin(m_channelVolumes), std::end(m_channelVolumes), 1.0f);

	m_frameCounter.reset(new FrameCounter());

	for (auto& pulse : m_pulseChannels)
	{
		pulse.reset(new PulseChannel());

		// Connect FrameCounter to chips it clocks
		m_frameCounter->AddConnection(pulse->m_volumeEnvelope);
		m_frameCounter->AddConnection(pulse->m_lengthCounter);
		m_frameCounter->AddConnection(pulse->m_sweepUnit);
	}

	// Pulse1 subtracts an extra 1 when computing target period
	m_pulseChannels[0]->m_sweepUnit.SetSubtractExtra();

	m_triangleChannel.reset(new TriangleChannel());
	m_frameCounter->AddConnection(m_triangleChannel->m_linearCounter);
	m_frameCounter->AddConnection(m_triangleChannel->m_lengthCounter);

	m_noiseChannel.reset(new NoiseChannel());
	m_frameCounter->AddConnection(m_noiseChannel->m_volumeEnvelope);
	m_frameCounter->AddConnection(m_noiseChannel->m_lengthCounter);

	m_elapsedCpuCycles = 0;
	
	m_audioDriver = std::make_shared<AudioDriver>();
	m_audioDriver->Initialize();
}

void Apu::Reset()
{
	HandleCpuWrite(0x4017, 0);
	HandleCpuWrite(0x4015, 0);
	for (uint16 address = 0x4000; address <= 0x400F; ++address)
		HandleCpuWrite(address, 0);
}

void Apu::Execute(uint32 cpuCycles)
{
	//@HACK: This is an attempt to determine how many CPU cycles must elapse before generating a sample.
	// It's based on PPU timing becaue that currently drives the frame-based rendering. It's not perfect,
	// though, because the PPU cycles per screen depends on whether rendering is enabled or not.
	const float64 kAvgNumScreenPpuCycles = 89342 - 0.5; // 1 less every odd frame when rendering is enabled
	const float64 kCpuCyclesPerSec = (kAvgNumScreenPpuCycles / 3) * 60.0;
	const float64 kCpuCyclesPerSample = kCpuCyclesPerSec / (float64)m_audioDriver->GetSampleRate();

	for (uint32 i = 0; i < cpuCycles; ++i)
	{
		m_frameCounter->Clock();

		// Clock all timers
		{
			m_triangleChannel->m_timer.Clock();

			// All other timers are clocked every 2nd CPU cycle (every APU cycle)
			if (m_evenFrame)
			{
				for (auto& pulse : m_pulseChannels)
					pulse->m_timer.Clock();

				m_noiseChannel->m_timer.Clock();
			}
			m_evenFrame = !m_evenFrame;
		}

		// Fill the sample buffer at the current output sample rate (i.e. 48 KHz)
		if (++m_elapsedCpuCycles >= kCpuCyclesPerSample)
		{
			m_elapsedCpuCycles -= kCpuCyclesPerSample;

			static float kMasterVolume = 1.0f;

			const float32 pulse1 = m_pulseChannels[0]->GetValue() * m_channelVolumes[ApuChannel::Pulse1];
			const float32 pulse2 = m_pulseChannels[1]->GetValue() * m_channelVolumes[ApuChannel::Pulse2];
			const float32 triangle = m_triangleChannel->GetValue() * m_channelVolumes[ApuChannel::Triangle];
			const float32 noise = m_noiseChannel->GetValue() * m_channelVolumes[ApuChannel::Noise];
			const float32 dmc = 0.0f;

			// Linear approximation
			const float32 pulseOut = 0.00752f * (pulse1 + pulse2);
			const float32 tndOut = 0.00851f * triangle + 0.00494f * noise + 0.00335f * dmc;
			const float32 sample = kMasterVolume * (pulseOut + tndOut);

			m_audioDriver->AddSampleF32(sample);
		}
	}
}

uint8 Apu::HandleCpuRead(uint16 cpuAddress)
{
	Bitfield<uint8> result;
	result.ClearAll();

	switch (cpuAddress)
	{
	case 0x4015:
		//@TODO: set bits 7,6,4: DMC interrupt (I), frame interrupt (F), DMC active (D)
		//@TODO: Reading this register clears the frame interrupt flag (but not the DMC interrupt flag).

		result.SetPos(0, m_pulseChannels[0]->m_lengthCounter.GetValue() > 0);
		result.SetPos(1, m_pulseChannels[1]->m_lengthCounter.GetValue() > 0);
		result.SetPos(2, m_triangleChannel->m_lengthCounter.GetValue() > 0);
		result.SetPos(3, m_noiseChannel->m_lengthCounter.GetValue() > 0);
		break;
	}
	
	return result.Value();
}

void Apu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	PulseChannel* pulse = nullptr;
	if (cpuAddress <= 0x4007)
	{
		pulse = m_pulseChannels[cpuAddress < 0x4004? 0 : 1].get();
		assert(pulse);
	}

	TriangleChannel* triangle = m_triangleChannel.get();
	NoiseChannel* noise = m_noiseChannel.get();

	switch (cpuAddress)
	{
		/////////////////////
		// Pulse 1 and 2
		/////////////////////
	case 0x4000:
	case 0x4004:
		pulse->m_pulseWaveGenerator.SetDuty( ReadBits(value, BITS(6, 7)) >> 6 );
		pulse->m_lengthCounter.SetHalt( TestBits(value, BIT(5)) );
		pulse->m_volumeEnvelope.SetLoop( TestBits(value, BIT(5)) ); // Same bit for length counter halt and envelope loop
		pulse->m_volumeEnvelope.SetConstantVolumeMode( TestBits(value, BIT(4)) );
		pulse->m_volumeEnvelope.SetConstantVolume( ReadBits(value, BITS(0,1,2,3)) );
		break;

	case 0x4001: // Sweep unit setup
	case 0x4005:
		pulse->m_sweepUnit.SetEnabled( TestBits(value, BIT(7)) );
		pulse->m_sweepUnit.SetPeriod( ReadBits(value, BITS(4,5,6)) >> 4 );
		pulse->m_sweepUnit.SetNegate( TestBits(value, BIT(3)) );
		pulse->m_sweepUnit.SetShiftCount( ReadBits(value, BITS(0,1,2)) );
		pulse->m_sweepUnit.Restart(); // Side effect
		break;

	case 0x4002:
	case 0x4006:
		pulse->m_timer.SetPeriodLow8(value);
		break;

	case 0x4003:
	case 0x4007:
		pulse->m_timer.SetPeriodHigh3( ReadBits(value, BITS(0,1,2)) );
		pulse->m_lengthCounter.LoadCounterFromLUT( ReadBits(value, BITS(3,4,5,6,7)) >> 3 );

		// Side effects...
		pulse->m_volumeEnvelope.Restart();
		pulse->m_pulseWaveGenerator.Restart(); //@TODO: for pulse channels the phase is reset - IS THIS RIGHT?
		break;

		/////////////////////
		// Triangle channel
		/////////////////////
	case 0x4008:
		triangle->m_lengthCounter.SetHalt( TestBits(value, BIT(7)) );
		triangle->m_linearCounter.SetControlAndPeriod( TestBits(value, BIT(7)), ReadBits(value, BITS(0,1,2,3,4,5,6)) );
		break;

	case 0x400A:
		triangle->m_timer.SetPeriodLow8(value);
		break;

	case 0x400B:
		triangle->m_timer.SetPeriodHigh3( ReadBits(value, BITS(0,1,2)) );
		triangle->m_linearCounter.Restart(); // Side effect
		triangle->m_lengthCounter.LoadCounterFromLUT(value >> 3);
		break;

		/////////////////////
		// Noise channel
		/////////////////////
	case 0x400C:
		noise->m_lengthCounter.SetHalt(TestBits(value, BIT(5)));
		noise->m_volumeEnvelope.SetConstantVolumeMode(TestBits(value, BIT(4)));
		noise->m_volumeEnvelope.SetConstantVolume(ReadBits(value, BITS(0,1,2,3)));
		break;

	case 0x400E:
		noise->m_shiftRegister.m_mode = TestBits(value, BIT(7));

		//@TODO: Move this into helper function SetNoiseTimerPeriod()
		{
			size_t ntscPeriods[] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
			static_assert(ARRAYSIZE(ntscPeriods) == 16, "Size error");
			//size_t palPeriods[] = { 4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 };
			//static_assert(ARRAYSIZE(palPeriods) == 16, "Size error");
			size_t index = ReadBits(value, BITS(0,1,2,3));
			noise->m_timer.SetPeriod(ntscPeriods[index]);
		}
		break;

	case 0x400F:
		noise->m_lengthCounter.LoadCounterFromLUT(value >> 3);
		noise->m_volumeEnvelope.Restart();
		break;

		/////////////////////
		// Misc
		/////////////////////
	case 0x4015:
		m_pulseChannels[0]->m_lengthCounter.SetEnabled( TestBits(value, BIT(0)) );
		m_pulseChannels[1]->m_lengthCounter.SetEnabled( TestBits(value, BIT(1)) );
		triangle->m_lengthCounter.SetEnabled( TestBits(value, BIT(2)) );
		noise->m_lengthCounter.SetEnabled( TestBits(value, BIT(3)) );
		//@TODO: DMC Enable bit 4
		break;

	case 0x4017:
		m_frameCounter->SetMode(ReadBits(value, BIT(7)) >> 7);

		if (TestBits(value, BIT(6)))
			m_frameCounter->AllowInterrupt(); //@TODO: double-check this
		break;
	}
}

void Apu::SetChannelVolume(ApuChannel::Type type, float32 volume)
{
	m_channelVolumes[type] = Clamp(volume, 0.0f, 1.0f);
}

void DebugDrawAudio(SDL_Renderer* renderer)
{
	(void)renderer;
	int x = 0;
	int y = 0;

	auto DrawBar = [&](float32 ratio, const Color4& color)
	{
		const int width = 400;
		const int height = 20;

		static auto white = Color4::White();
		SDL_Rect rect = { x, y, width, height };
		SDL_SetRenderDrawColor(renderer, white.R(), white.G(), white.B(), white.A());
		SDL_RenderFillRect(renderer, &rect);

		SDL_Rect rect2 = { x, y, (int)(ratio * width), 20 };
		SDL_SetRenderDrawColor(renderer, color.R(), color.G(), color.B(), color.A());
		SDL_RenderFillRect(renderer, &rect2);

		y += (height + 1);
	};

#if 0
	auto pulse = g_apu->m_pulseChannels[1].get();

	DrawBar(pulse->GetValue() / 15.0f, Color4::Yellow());
	DrawBar(pulse->m_volumeEnvelope.GetVolume() / 15.0f, Color4::Red());
	DrawBar(pulse->m_volumeEnvelope.m_counter / 15.0f, Color4::Cyan());
	DrawBar(pulse->m_volumeEnvelope.m_constantVolume / 15.0f, Color4::Magenta());
	DrawBar(pulse->m_sweepUnit.SilenceChannel() ? 0 : 1, Color4::Green());
	DrawBar(pulse->m_pulseWaveGenerator.GetValue(), Color4::Blue());
	DrawBar(pulse->m_lengthCounter.GetValue() / 255.0f, Color4::Black());

	DrawBar(pulse->m_sweepUnit.m_divider.GetCounter() / 7.0f, Color4::Red());
	DrawBar(pulse->m_timer.m_divider.GetPeriod() / 2047.0f, Color4::Blue());
	DrawBar(pulse->m_timer.m_divider.GetCounter() / 2047.0f, Color4::Green());

	DrawBar(g_apu->m_audioDriver->GetBufferUsageRatio(), Color4::Green());
#endif
}
