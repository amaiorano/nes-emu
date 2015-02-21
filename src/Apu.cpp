#include "Apu.h"
#include "AudioDriver.h"
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
	void Initialize()
	{
		m_period = 0;
		m_counter = 0;
	}

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
	void Initialize()
	{
		m_enabled = false;
		m_halt = false;
		m_counter = 0;
	}

	void SetEnabled(bool enabled)
	{
		m_enabled = enabled;
		if (!m_enabled)
			m_counter = 0;
	}

	void SetHalt(bool halt)
	{
		m_halt = halt;
	}

	//@Side Effects of load: The envelope is restarted, for pulse channels phase is reset,
	// for triangle the linear counter reload flag is set.
	void LoadCounterFromLUT(uint8 index)
	{
		static uint8 lut[] = 
		{ 
			10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
			12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
		};
		static_assert(ARRAYSIZE(lut) == 32, "Invalid");
		assert(index < ARRAYSIZE(lut));

		if (m_enabled)
			m_counter = lut[index];
	}

	// Clocked by FrameCounter
	void Clock()
	{
		if (m_counter > 0 && !m_halt)
			--m_counter;
	}

	size_t ReadCounter() const
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
	void Initialize()
	{
		m_restart = true;
		m_loop = false;
		m_divider.Initialize();
		m_counter = 0;
		m_constantVolumeMode = false;
		m_constantVolume = 0;
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

	size_t GetVolume()
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

// Produces the square wave based on one of 4 duty cycles
// http://wiki.nesdev.com/w/index.php/APU_Pulse
class PulseWaveGenerator
{
public:
	void Initialize()
	{
		m_duty = 0;
		m_step = 0;
	}

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
	void Clock()
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
	void Initialize()
	{
		m_divider.Initialize();
		m_pulseWaveGenerator = nullptr;
	}

	void Connect(PulseWaveGenerator& pulseWaveGenerator)
	{
		assert(m_pulseWaveGenerator == nullptr);
		m_pulseWaveGenerator = &pulseWaveGenerator;
	}

	void Reset()
	{
		m_divider.ResetCounter();
	}

	size_t GetPeriod() const { return m_divider.GetPeriod(); }

	void SetPeriod(size_t period)
	{
		assert(period < (BIT(11) - 1));
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
		assert(value < (BIT(3) - 1));
		size_t period = m_divider.GetPeriod();
		period = (value << 8) | (period & 0xFF); // Keep low 8 bits
		m_divider.SetPeriod(period);

		m_divider.ResetCounter();
	}

	// Clocked by CPU clock every cycle (triangle channel) or second cycle (pulse/noise channels)
	void Clock()
	{
		if (m_divider.Clock())
		{
			m_pulseWaveGenerator->Clock();
		}
	}

private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	Divider m_divider;
	PulseWaveGenerator* m_pulseWaveGenerator;
};

// Periodically adjusts the period of the Timer, sweeping the frequency high or low over time
// http://wiki.nesdev.com/w/index.php/APU_Sweep
class SweepUnit
{
public:
	void Initialize()
	{
		m_subtractExtra = 0;
		m_enabled = true;
		m_negate = false;
		m_reload = false;
		m_silenceChannel = false;
		m_shiftCount = 0;
		m_divider.Initialize();
		m_timer = nullptr;
		m_targetPeriod = 0;
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
		assert(shiftCount < (BIT(2) - 1));
		m_shiftCount = shiftCount;
	}

	void Restart() { m_reload = true;  }

	// Clocked by FrameCounter
	void Clock()
	{
		ComputeTargetPeriod();

		if (m_reload)
		{
			//const auto lastDividerCounter = m_divider.GetCounter();

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
			// Pulse1 subtracts an extra 1, but not Pulse2
			m_targetPeriod = currPeriod - shiftedPeriod - m_subtractExtra;
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
	void Initialize()
	{
		m_volumeEnvelope.Initialize();
		m_sweepUnit.Initialize();
		m_timer.Initialize();
		m_pulseWaveGenerator.Initialize();
		m_lengthCounter.Initialize();

		// Connect timer to sequencer
		m_timer.Connect(m_pulseWaveGenerator);

		// Connect sweep unit to timer
		m_sweepUnit.Connect(m_timer);
	}

	size_t GetValue()
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


// aka Frame Sequencer
// http://wiki.nesdev.com/w/index.php/APU_Frame_Counter
class FrameCounter
{
public:
	void Initialize()
	{
		m_cpuCycles = 0;
		m_numSteps = 4;
		m_inhibitInterrupt = true;
	}

	void AddConnection(VolumeEnvelope& e) { m_envelopes.push_back(&e); }
	void AddConnection(LengthCounter& lc) { m_lengthCounters.push_back(&lc); }
	void AddConnection(SweepUnit& su) { m_sweepUnits.push_back(&su); }

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
		//@TODO: triangle's linear counter
		ClockAll(m_envelopes);
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
};

void Apu::Initialize()
{
	g_apu = this;

	m_evenFrame = true;

	m_frameCounter.reset(new FrameCounter());
	m_frameCounter->Initialize();

	for (auto& pulse : m_pulseChannels)
	{
		pulse.reset(new PulseChannel());
		pulse->Initialize();

		// Connect FrameCounter to chips it clocks
		m_frameCounter->AddConnection(pulse->m_volumeEnvelope);
		m_frameCounter->AddConnection(pulse->m_lengthCounter);
		m_frameCounter->AddConnection(pulse->m_sweepUnit);
	}

	// Pulse1 subtracts an extra 1 when computing target period
	m_pulseChannels[0]->m_sweepUnit.SetSubtractExtra();

	m_elapsedCpuCycles = 0;
	
	m_audioDriver = std::make_shared<AudioDriver>();
	m_audioDriver->Initialize();
}

void Apu::Execute(uint32 cpuCycles)
{
	const float32 kCpuCyclesPerSec = 1786840.0f;
	const float32 kCpuCyclesPerSample = kCpuCyclesPerSec / (float32)m_audioDriver->GetSampleRate();

	for (uint32 i = 0; i < cpuCycles; ++i)
	{
		m_frameCounter->Clock();

		//@TODO: Clock all timers - pulse timers are clocked every 2nd CPU clock (even frames)
		if (m_evenFrame)
		{
			for (auto& pulse : m_pulseChannels)
				pulse->m_timer.Clock();
		}
		m_evenFrame = !m_evenFrame;

		// Fill the sample buffer at the current output sample rate (i.e. 48 KHz)
		if (++m_elapsedCpuCycles >= kCpuCyclesPerSample)
		{
			m_elapsedCpuCycles -= kCpuCyclesPerSample;

			static float MasterVolume = 0.5f;

			const size_t pulse1 = m_pulseChannels[0]->GetValue();
			const size_t pulse2 = m_pulseChannels[1]->GetValue();

			const float32 totalPulse = static_cast<float32>(pulse1 + pulse2);
			const float32 pulseOut = totalPulse == 0 ? 0 : 95.88f / ((8128.0f / totalPulse) + 100);

			const float32 sample = MasterVolume * pulseOut;

			m_audioDriver->AddSampleF32(sample);
		}
	}
}

uint8 Apu::HandleCpuRead(uint16 cpuAddress)
{
	(void)cpuAddress;
	return 0;
}

void Apu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	PulseChannel* pulse = nullptr;
	if (cpuAddress <= 0x4007)
	{
		pulse = m_pulseChannels[cpuAddress < 0x4004? 0 : 1].get();
		assert(pulse);
	}

	switch (cpuAddress)
	{
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

	case 0x4015:
		m_pulseChannels[0]->m_lengthCounter.SetEnabled( TestBits(value, BIT(0)) );
		m_pulseChannels[1]->m_lengthCounter.SetEnabled( TestBits(value, BIT(1)) );
		//@TODO: bits 1, 2, 3 set length counter values for other 3 channels
		break;

	case 0x4017:
		m_frameCounter->SetMode(ReadBits(value, BIT(7)) >> 7);

		if (TestBits(value, BIT(6)))
			m_frameCounter->AllowInterrupt(); //@TODO: double-check this
		break;
	}
}

void DebugDrawAudio(SDL_Renderer* renderer)
{
	(void)renderer;
#if 0
	int x = 0;
	int y = 0;

	auto DrawBar = [&](float size, const Color4& color)
	{
		SDL_Rect rect = { x, y, (int)size, 20 };
		y += 20;

		SDL_SetRenderDrawColor(renderer, color.R(), color.G(), color.B(), color.A());
		SDL_RenderFillRect(renderer, &rect);
	};

	float maxWidth = 200.0f;
	auto pulse = g_apu->m_pulseChannels[1].get();

	DrawBar(pulse->GetValue() / 15.0f * maxWidth, Color4::White());
	DrawBar(pulse->m_volumeEnvelope.GetVolume() / 15.0f * maxWidth, Color4::Red());
	DrawBar(pulse->m_volumeEnvelope.m_counter / 15.0f * maxWidth, Color4::Cyan());
	DrawBar(pulse->m_volumeEnvelope.m_constantVolume / 15.0f * maxWidth, Color4::Magenta());
	DrawBar(pulse->m_sweepUnit.SilenceChannel() ? 0 : maxWidth, Color4::Green());
	DrawBar(pulse->m_pulseWaveGenerator.GetValue() * maxWidth, Color4::Blue());
	DrawBar(pulse->m_lengthCounter.ReadCounter() / 255.0f * maxWidth, Color4::Black());

	DrawBar(pulse->m_sweepUnit.m_divider.GetCounter() / 7.0f * maxWidth, Color4::Red());
	DrawBar(pulse->m_timer.m_divider.GetPeriod() / 2047.0f * maxWidth, Color4::Blue());
	DrawBar(pulse->m_timer.m_divider.GetCounter() / 2047.0f * maxWidth, Color4::Green());
#endif
}
