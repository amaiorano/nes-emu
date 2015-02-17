#include "Apu.h"
#include "AudioDriver.h"
#include <vector>
#include <algorithm>

// Temp for debug drawing
#include "Renderer.h"
#include <SDL_render.h>

#define APU_TO_CPU_CYCLE(cpuCycle) static_cast<size_t>(cpuCycle * 2)

Apu* g_apu = nullptr; //@HACK: get rid of this

struct Divider
{
	size_t m_period;
	size_t m_counter;

	void Initialize()
	{
		SetPeriod(0);
	}

	void SetPeriod(size_t period)
	{
		m_period = period;
		m_counter = m_period;
	}

	void ResetCounter()
	{
		m_counter = m_period;
	}

	bool Clock()
	{
		//@NOTE: Can't assert this because period is written in 2 writes with a potential Clock in between.
		// Can fix this by making sure to call SetPeriod only once both writes are made?
		//assert(m_counter > 0 || m_period == 0);

		if (m_counter > 0 && --m_counter == 0)
		{
			ResetCounter();
			return true;
		}
		return false;
	}	
};


// When LengthCounter reaches 0, corresponding channel is silenced
// file:///C:/code/tony/nes-emu-temp/nesdevwiki/wikipages/APU_Length_Counter.xhtml
struct LengthCounter
{
	bool m_enabled;
	bool m_halt;
	size_t m_counter;

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

	// If 0 returned, should silence channel
	size_t ReadCounter() const
	{
		return m_counter;
	}
};

// Controls volume in 2 ways: decreasing saw with optional looping, or constant volume
// Input: Clocked by Frame Sequencer
// Output: 4-bit volume value (0-15)
// http://wiki.nesdev.com/w/index.php/APU_Envelope
struct Envelope
{
	bool m_startFlag;
	bool m_loop;
	Divider m_divider;
	size_t m_counter; // Saw envelope volume value (if not constant volume mode)
	bool m_constantVolumeMode;
	size_t m_constantVolume; // Also reload value for divider

	void Initialize()
	{
		m_startFlag = true;
		m_loop = false;
		m_divider.Initialize();
		m_counter = 0;
		m_constantVolume = false;
		m_constantVolume = 0;
	}

	void SetConstantVolume(uint16 value)
	{
		assert(value < 16);
		m_constantVolume = value;
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
		if (m_startFlag)
		{
			m_startFlag = false;
			m_counter = 15;
			m_divider.SetPeriod(m_constantVolume + 1); // Constant volume doubles up as divider reload value (+1)
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
};

// Produces the square wave based on one of 4 duty cycles
struct PulseSequencer
{
	uint8 m_duty; // 2 bits
	uint8 m_step; // 0-7
	uint8 m_currValue;

	void Initialize()
	{
		m_duty = 0;
		m_step = 0;
		m_currValue = 0;
	}

	void SetDuty(uint8 duty)
	{
		assert(duty < 4);
		m_duty = duty;
	}

	// Clocked by an Timer, outputs bit (0 or 1)
	void Clock()
	{
		static uint8 sequences[4][8] =
		{
			{ 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
			{ 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
			{ 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
			{ 1, 0, 0, 1, 1, 1, 1, 1 }  // 25% negated
		};

		m_step = (m_step + 1) % 8;
		
		const uint8 value = sequences[m_duty][m_step];
		// @TODO: This value is fed into a gate? Or we just return it...
		m_currValue = value;
	}
};

// A timer is used in each of the five channels to control the sound frequency. It contains a divider which is
// clocked by the CPU clock. The triangle channel's timer is clocked on every CPU cycle, but the pulse, noise,
// and DMC timers are clocked only on every second CPU cycle and thus produce only even periods.
// **** Don't need this.. just use a divider for the timer part (divider -> divider for non-triangle)
struct Timer
{
	Divider m_divider; // Clocked by CPU clock...
	PulseSequencer* m_sequencer;

	void Initialize()
	{
		m_divider.Initialize();
		m_sequencer = nullptr;
	}

	void SetCounterLow8(uint8 value)
	{
		//@NOTE: Bypassing SetPeriod
		m_divider.m_period = value;
	}

	void SetCounterHigh3(uint8 value)
	{
		//@NOTE: Bypassing SetPeriod
		m_divider.m_period |= (value << 8);
		m_divider.ResetCounter();
	}

	// Clocked by CPU clock every cycle (triangle channel) or second cycle (pulse/noise channels)
	void Clock()
	{
		if (m_divider.Clock())
		{
			m_sequencer->Clock();
		}
	}
};

// http://wiki.nesdev.com/w/index.php/APU_Sweep
struct SweepUnit
{
	bool m_enabled;
	bool m_negate;
	bool m_reload;
	bool m_silenceChannel; // This is the Sweep -> Gate connection, if true channel is silenced
	uint8 m_shiftCount; // [0,7]
	Divider m_divider;
	Timer* m_timer;
	size_t m_targetPeriod; // Target period for the timer; is computed continuously in real hardware

	void Initialize()
	{
		m_enabled = true;
		m_negate = false;
		m_reload = false;
		m_silenceChannel = false;
		m_shiftCount = 0;
		m_divider.Initialize();
		m_timer = nullptr;
		m_targetPeriod = 0;
	}

	void SetPeriod(size_t period)
	{
		assert(period < 8); // 3 bits
		m_divider.SetPeriod(period + 1); //@TODO: Are we only supposed to set the period? Or is it correct to reset the counter as well?
		ComputeTargetPeriod();
	}

	// Clocked by FrameCounter
	void Clock()
	{
		ComputeTargetPeriod();

		if (m_reload)
		{
			const auto lastDividerCounter = m_divider.m_counter;
			m_divider.ResetCounter();

			if (lastDividerCounter == 0 && m_enabled)
			{
				//ComputeTargetPeriod();
				AdjustTimerPeriod();
			}

			m_reload = false;
		}
		else
		{
			if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod();
			}
		}
	}

	template <typename T>
	static inline T BitwiseRotateRight(T value, T shift, T width)
	{
		const T mask = (1 << shift) - 1; // Convert number of bits to shift to a mask with that many bits set
		const T chopped = value & mask;
		const T result = (chopped << (width - shift)) | (value >> shift);
		return result;
	}

	void ComputeTargetPeriod()
	{
		//@TODO: Validate this code. Not sure if this is what the wiki describes.
		auto& currPeriod = m_timer->m_divider.m_period;
		//const size_t shiftedPeriod = BitwiseRotateRight<size_t>(currPeriod, m_shiftCount, 11);
		const size_t shiftedPeriod = currPeriod >> m_shiftCount;

		if (m_negate)
		{
			//@TODO: For Pulse1, subtract an extra 1 from here
			m_targetPeriod = currPeriod - shiftedPeriod;
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
			auto& currPeriod = m_timer->m_divider.m_period;
			currPeriod = m_targetPeriod;
		}
	}
};

struct PulseChannel
{
	void Initialize()
	{
		m_envelope.Initialize();
		m_sweepUnit.Initialize();
		m_timer.Initialize();
		m_sequencer.Initialize();
		m_lengthCounter.Initialize();

		// Connect timer to sequencer
		m_timer.m_sequencer = &m_sequencer;

		// Connect sweep unit to timer
		m_sweepUnit.m_timer = &m_timer;
	}

	size_t GetValue()
	{
		//@TODO: Maybe we should use gates

		if (m_sweepUnit.m_silenceChannel)
			return 0;

		if (m_lengthCounter.ReadCounter() == 0)
			return 0;

		//auto value = std::max<size_t>(5, m_envelope.GetVolume()) * m_sequencer.m_currValue;
		auto value = m_envelope.GetVolume() * m_sequencer.m_currValue;

		assert(value < 16);
		return value;
	}

	Envelope m_envelope; // Clocked by FrameCounter
	SweepUnit m_sweepUnit; // Clocked by FrameCounter
	Timer m_timer; // Clocked by CPU clock
	PulseSequencer m_sequencer; // Clocked by Timer
	LengthCounter m_lengthCounter; // Clocked by FrameCounter
};


// aka Frame Sequencer
// file:///C:/code/tony/nes-emu-temp/nesdevwiki/wikipages/APU_Frame_Counter.xhtml
struct FrameCounter
{
	size_t m_cpuCycles;
	size_t m_numSteps;
	bool m_inhibitInterrupt;

	std::vector<Envelope*> m_envelopes;
	std::vector<LengthCounter*> m_lengthCounters;
	std::vector<SweepUnit*> m_sweepUnits;

	void Initialize()
	{
		m_cpuCycles = 0;
		m_numSteps = 4;
		m_inhibitInterrupt = true;
	}

	template <typename Container>
	void ClockAll(Container& container)
	{
		for (auto& curr : container)
		{
			curr->Clock();
		}
	}

	// Clock every CPU cycle
	void Clock()
	{
		m_cpuCycles += 1;

		switch (m_cpuCycles)
		{
		case APU_TO_CPU_CYCLE(3728.5):
			//@TODO: triangle's linear counter
			ClockAll(m_envelopes);
			break;

		case APU_TO_CPU_CYCLE(7456.5):
			//@TODO: triangle's linear counter
			ClockAll(m_envelopes);
			ClockAll(m_lengthCounters);
			ClockAll(m_sweepUnits);
			break;

		case APU_TO_CPU_CYCLE(11185.5):
			//@TODO: triangle's linear counter
			ClockAll(m_envelopes);
			break;

		case APU_TO_CPU_CYCLE(14914):
			//@TODO: set interrupt flag if !inhibit
			break;

		case APU_TO_CPU_CYCLE(14914.5):
			if (m_numSteps == 4)
			{
				//@TODO: triangle's linear counter
				//@TODO: set interrupt flag if !inhibit
				ClockAll(m_envelopes);
				ClockAll(m_lengthCounters);
				ClockAll(m_sweepUnits);
			}
			break;

		case APU_TO_CPU_CYCLE(14915):
			if (m_numSteps == 4)
			{
				//@TODO: set interrupt flag if !inhibit

				// Reset counter
				m_cpuCycles = 0;
			}
			break;

		case APU_TO_CPU_CYCLE(18640.5):
			if (m_numSteps == 5)
			{
				//@TODO: triangle's linear counter
				ClockAll(m_envelopes);
				ClockAll(m_lengthCounters);
				ClockAll(m_sweepUnits);
			}
			break;

		case APU_TO_CPU_CYCLE(18641):
			if (m_numSteps == 5)
			{
				// Reset counter
				m_cpuCycles = 0;
			}
			break;
		}
	}
};

void Apu::Initialize()
{
	g_apu = this;

	m_evenFrame = true;

	m_frameCounter.reset(new FrameCounter());
	m_frameCounter->Initialize();

	m_pulse1.reset(new PulseChannel());
	m_pulse1->Initialize();
	
	// Connect FrameCounter to chips it clocks
	//@TODO: pass in FrameCounter to PulseChannel::Initialize so it can do this
	m_frameCounter->m_envelopes.push_back(&m_pulse1->m_envelope);
	m_frameCounter->m_lengthCounters.push_back(&m_pulse1->m_lengthCounter);
	m_frameCounter->m_sweepUnits.push_back(&m_pulse1->m_sweepUnit);

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
		//@TODO: Clock all timers - pulse timers are clocked every 2nd CPU clock (even frames)
		if (m_evenFrame)
		{
			m_pulse1->m_timer.Clock();
		}

		m_frameCounter->Clock();

		m_evenFrame = !m_evenFrame;

		// Fill the sample buffer at the current output sample rate (i.e. 48 KHz)
		if (++m_elapsedCpuCycles >= kCpuCyclesPerSample)
		{
			m_elapsedCpuCycles -= kCpuCyclesPerSample;

			float32 sample = m_pulse1->GetValue() / 15.0f;
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
	auto pulse1 = m_pulse1.get();

	switch (cpuAddress)
	{
	case 0x4000:
		pulse1->m_sequencer.SetDuty( ReadBits(value, BITS(6, 7)) >> 6 );
		pulse1->m_lengthCounter.m_halt = TestBits(value, BIT(5));
		pulse1->m_envelope.m_loop = pulse1->m_lengthCounter.m_halt; // Same bit
		pulse1->m_envelope.m_constantVolumeMode = TestBits(value, BIT(4));
		pulse1->m_envelope.m_constantVolume = ReadBits(value, BITS(0,1,2,3));
		break;

	case 0x4001: // Sweep unit setup
		pulse1->m_sweepUnit.m_enabled = TestBits(value, BIT(7));
		pulse1->m_sweepUnit.m_divider.SetPeriod((ReadBits(value, BITS(4, 5, 6)) >> 4) + 1); //@TODO: Call func on sweep unit
		pulse1->m_sweepUnit.m_negate = TestBits(value, BIT(3));
		pulse1->m_sweepUnit.m_shiftCount = ReadBits(value, BITS(0,1,2));
		pulse1->m_sweepUnit.m_reload = true; // Side effect
		break;

	case 0x4002:
		pulse1->m_timer.SetCounterLow8(value);
		break;

	case 0x4003:
		pulse1->m_timer.SetCounterHigh3( ReadBits(value, BITS(0,1,2)) );
		pulse1->m_lengthCounter.LoadCounterFromLUT( ReadBits(value, BITS(3,4,5,6,7)) >> 3 );

		// Side effects...
		pulse1->m_envelope.m_startFlag = true;
		pulse1->m_sequencer.m_step = 0; //@TODO: for pulse channels the phase is reset - IS THIS RIGHT?
		break;

	case 0x4015:
		pulse1->m_lengthCounter.SetEnabled( TestBits(value, BIT(0)) );
		//@TODO: bits 1, 2, 3 set length counter values for other 3 channels
		break;
	}
}

void DebugDrawAudio(SDL_Renderer* renderer)
{
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
	auto pulse1 = g_apu->m_pulse1.get();

	DrawBar(pulse1->GetValue() / 15.0f * maxWidth, Color4::White());
	DrawBar(pulse1->m_envelope.GetVolume() / 15.0f * maxWidth, Color4::Red());
	DrawBar(pulse1->m_sweepUnit.m_silenceChannel ? 0 : maxWidth, Color4::Green());
	DrawBar(pulse1->m_sequencer.m_currValue * maxWidth, Color4::Blue());
	DrawBar(pulse1->m_lengthCounter.ReadCounter() / 255.0f * maxWidth, Color4::Black());
}

