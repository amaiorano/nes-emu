#include "Apu.h"
#include "AudioDriver.h"
#include "Bitfield.h"
#include "Serializer.h"
#include <vector>
#include <algorithm>

// Temp for debug drawing
#include "Renderer.h"
#include <SDL_render.h>
Apu* g_apu = nullptr; //@HACK: get rid of this

// If set, samples every CPU cycle (~1.79 MHz, more expensive but better quality),
// otherwise will only sample at output rate (e.g. 44.1 KHz)
#define SAMPLE_EVERY_CPU_CYCLE 1

class LengthCounter;

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

// Produces the square wave based on one of 4 duty cycles
// http://wiki.nesdev.com/w/index.php/APU_Pulse
class PulseWaveGenerator
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
	Timer() : m_minPeriod(0) {}

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
	// Returns true when output chip should be clocked
	bool Clock()
	{
		// Avoid popping and weird noises from ultra sonic frequencies
		if (m_divider.GetPeriod() < m_minPeriod)
			return false;

		if (m_divider.Clock())
		{
			return true;
		}
		return false;
	}

private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	Divider m_divider;
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
		, m_targetPeriod(0)
	{
	}

	void SetSubtractExtra()
	{
		m_subtractExtra = 1;
	}

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	void SetNegate(bool negate) { m_negate = negate; }

	void SetPeriod(size_t period, Timer& timer)
	{
		assert(period < 8); // 3 bits
		m_divider.SetPeriod(period); // Don't reset counter

		// From wiki: The adder computes the next target period immediately after the period is updated by $400x writes
		// or by the frame counter.
		ComputeTargetPeriod(timer);
	}

	void SetShiftCount(uint8 shiftCount)
	{
		assert(shiftCount < BIT(3));
		m_shiftCount = shiftCount;
	}

	void Restart() { m_reload = true;  }

	// Clocked by FrameCounter
	void Clock(Timer& timer)
	{
		ComputeTargetPeriod(timer);

		if (m_reload)
		{
			// From nesdev wiki: "If the divider's counter was zero before the reload and the sweep is enabled,
			// the pulse's period is also adjusted". What this effectively means is: if the divider would have
			// clocked and reset as usual, adjust the timer period.
			if (m_enabled && m_divider.Clock())
			{
				AdjustTimerPeriod(timer);
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
				AdjustTimerPeriod(timer);
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
				AdjustTimerPeriod(timer);
			}
#endif
		}
	}

	bool SilenceChannel() const
	{
		return m_silenceChannel;
	}

private:
	void ComputeTargetPeriod(Timer& timer)
	{
		assert(m_shiftCount < 8); // 3 bits

		const size_t currPeriod = timer.GetPeriod();
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

	void AdjustTimerPeriod(Timer& timer)
	{
		// If channel is not silenced, it means we're in range
		if (m_enabled && m_shiftCount > 0 && !m_silenceChannel)
		{
			timer.SetPeriod(m_targetPeriod);
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
	size_t m_targetPeriod; // Target period for the timer; is computed continuously in real hardware
};

// Concrete base class for audio channels
class AudioChannel
{
public:
	LengthCounter& GetLengthCounter()
	{
		return m_lengthCounter;
	}

protected:
	Timer m_timer;
	LengthCounter m_lengthCounter;
};

// http://wiki.nesdev.com/w/index.php/APU_Pulse
class PulseChannel : public AudioChannel
{
public:
	PulseChannel(uint8 pulseChannelNumber)
	{
		assert(pulseChannelNumber < 2);
		if (pulseChannelNumber == 0)
			m_sweepUnit.SetSubtractExtra();
	}

	void ClockQuarterFrameChips()
	{
		m_volumeEnvelope.Clock();
	}

	void ClockHalfFrameChips()
	{
		m_lengthCounter.Clock();
		m_sweepUnit.Clock(m_timer);
	}

	void ClockTimer()
	{
		if (m_timer.Clock())
		{
			m_pulseWaveGenerator.Clock();
		}
	}

	void HandleCpuWrite(uint16 cpuAddress, uint8 value)
	{
		switch (ReadBits(cpuAddress, BITS(0,1)))
		{
		case 0:
			m_pulseWaveGenerator.SetDuty(ReadBits(value, BITS(6, 7)) >> 6);
			m_lengthCounter.SetHalt(TestBits(value, BIT(5)));
			m_volumeEnvelope.SetLoop(TestBits(value, BIT(5))); // Same bit for length counter halt and envelope loop
			m_volumeEnvelope.SetConstantVolumeMode(TestBits(value, BIT(4)));
			m_volumeEnvelope.SetConstantVolume(ReadBits(value, BITS(0, 1, 2, 3)));
			break;

		case 1: // Sweep unit setup
			m_sweepUnit.SetEnabled(TestBits(value, BIT(7)));
			m_sweepUnit.SetPeriod(ReadBits(value, BITS(4, 5, 6)) >> 4, m_timer);
			m_sweepUnit.SetNegate(TestBits(value, BIT(3)));
			m_sweepUnit.SetShiftCount(ReadBits(value, BITS(0, 1, 2)));
			m_sweepUnit.Restart(); // Side effect
			break;

		case 2:
			m_timer.SetPeriodLow8(value);
			break;

		case 3:
			m_timer.SetPeriodHigh3(ReadBits(value, BITS(0, 1, 2)));
			m_lengthCounter.LoadCounterFromLUT(ReadBits(value, BITS(3, 4, 5, 6, 7)) >> 3);

			// Side effects...
			m_volumeEnvelope.Restart();
			m_pulseWaveGenerator.Restart(); //@TODO: for pulse channels the phase is reset - IS THIS RIGHT?
			break;

		default:
			assert(false);
			break;
		}
	}

	size_t GetValue() const
	{
		if (m_sweepUnit.SilenceChannel())
			return 0;

		if (m_lengthCounter.SilenceChannel())
			return 0;

		auto value = m_volumeEnvelope.GetVolume() * m_pulseWaveGenerator.GetValue();

		assert(value < 16);
		return value;
	}

private:
	friend void DebugDrawAudio(SDL_Renderer* renderer);

	VolumeEnvelope m_volumeEnvelope;
	SweepUnit m_sweepUnit;
	PulseWaveGenerator m_pulseWaveGenerator;
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

class TriangleWaveGenerator
{
public:
	TriangleWaveGenerator() : m_step(0) {}

	void Clock()
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

class TriangleChannel : public AudioChannel
{
public:
	TriangleChannel()
	{
		m_timer.SetMinPeriod(2); // Avoid popping from ultrasonic frequencies
	}

	void ClockQuarterFrameChips()
	{
		m_linearCounter.Clock();
	}

	void ClockHalfFrameChips()
	{
		m_lengthCounter.Clock();
	}

	void ClockTimer()
	{
		if (m_timer.Clock())
		{
			if (m_linearCounter.GetValue() > 0 && m_lengthCounter.GetValue() > 0)
			{
				m_triangleWaveGenerator.Clock();
			}
		}
	}

	void HandleCpuWrite(uint16 cpuAddress, uint8 value)
	{
		switch (cpuAddress)
		{
		case 0x4008:
			m_lengthCounter.SetHalt(TestBits(value, BIT(7)));
			m_linearCounter.SetControlAndPeriod(TestBits(value, BIT(7)), ReadBits(value, BITS(0, 1, 2, 3, 4, 5, 6)));
			break;

		case 0x400A:
			m_timer.SetPeriodLow8(value);
			break;

		case 0x400B:
			m_timer.SetPeriodHigh3(ReadBits(value, BITS(0, 1, 2)));
			m_linearCounter.Restart(); // Side effect
			m_lengthCounter.LoadCounterFromLUT(value >> 3);
			break;

		default:
			assert(false);
			break;
		};
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
	TriangleWaveGenerator m_triangleWaveGenerator;
};

class LinearFeedbackShiftRegister
{
public:
	LinearFeedbackShiftRegister() : m_register(1), m_mode(false){}

	// Clocked by noise channel timer
	void Clock()
	{
		uint16 bit0 = ReadBits(m_register, BIT(0));

		uint16 whichBitN = m_mode ? 6 : 1;
		uint16 bitN = ReadBits(m_register, BIT(whichBitN)) >> whichBitN;

		uint16 feedback = bit0 ^ bitN;
		assert(feedback < 2);

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

class NoiseChannel : public AudioChannel
{
public:
	NoiseChannel()
	{
		m_volumeEnvelope.SetLoop(true); // Always looping
	}

	void ClockQuarterFrameChips()
	{
		m_volumeEnvelope.Clock();
	}

	void ClockHalfFrameChips()
	{
		m_lengthCounter.Clock();
	}

	void ClockTimer()
	{
		if (m_timer.Clock())
		{
			m_shiftRegister.Clock();
		}
	}

	size_t GetValue() const
	{
		if (m_shiftRegister.SilenceChannel() || m_lengthCounter.SilenceChannel())
			return 0;

		return m_volumeEnvelope.GetVolume();
	}

	void HandleCpuWrite(uint16 cpuAddress, uint8 value)
	{
		switch (cpuAddress)
		{
		case 0x400C:
			m_lengthCounter.SetHalt(TestBits(value, BIT(5)));
			m_volumeEnvelope.SetConstantVolumeMode(TestBits(value, BIT(4)));
			m_volumeEnvelope.SetConstantVolume(ReadBits(value, BITS(0, 1, 2, 3)));
			break;

		case 0x400E:
			m_shiftRegister.m_mode = TestBits(value, BIT(7));
			SetNoiseTimerPeriod(ReadBits(value, BITS(0, 1, 2, 3)));
			break;

		case 0x400F:
			m_lengthCounter.LoadCounterFromLUT(value >> 3);
			m_volumeEnvelope.Restart();
			break;

		default:
			assert(false);
			break;
		};
	}

private:
	void SetNoiseTimerPeriod(size_t lutIndex)
	{
		static size_t ntscPeriods[] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
		static_assert(ARRAYSIZE(ntscPeriods) == 16, "Size error");
		//size_t palPeriods[] = { 4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 };
		//static_assert(ARRAYSIZE(palPeriods) == 16, "Size error");

		assert(lutIndex < ARRAYSIZE(ntscPeriods));

		// The LUT contains the effective period for the channel, but the timer is clocked
		// every second CPU cycle so we divide by 2, and the divider's input is the period
		// reload value so we subtract by 1.
		const size_t periodReloadValue = (ntscPeriods[lutIndex] / 2) - 1;
		m_timer.SetPeriod(periodReloadValue);
	}

	VolumeEnvelope m_volumeEnvelope;
	LinearFeedbackShiftRegister m_shiftRegister;
};

// aka Frame Sequencer
// http://wiki.nesdev.com/w/index.php/APU_Frame_Counter
class FrameCounter
{
public:
	FrameCounter(Apu& apu)
		: m_apu(&apu)
		, m_cpuCycles(0)
		, m_numSteps(4)
		, m_inhibitInterrupt(true)
	{
	}

	void Serialize(class Serializer& serializer)
	{
		SERIALIZE(m_cpuCycles);
		SERIALIZE(m_numSteps);
		SERIALIZE(m_inhibitInterrupt);
	}

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

			//@TODO: This should happen in 3 or 4 CPU cycles
			ClockQuarterFrameChips();
			ClockHalfFrameChips();
		}

		// Always restart sequence
		//@TODO: This should happen in 3 or 4 CPU cycles
		m_cpuCycles = 0;
	}

	void AllowInterrupt() { m_inhibitInterrupt = false; }

	void HandleCpuWrite(uint16 cpuAddress, uint8 value)
	{
		(void)cpuAddress;
		assert(cpuAddress == 0x4017);

		SetMode(ReadBits(value, BIT(7)) >> 7);

		if (TestBits(value, BIT(6)))
			AllowInterrupt(); //@TODO: double-check this
	}

	// Clock every CPU cycle
	void Clock()
	{
		bool resetCycles = false;

		#define APU_TO_CPU_CYCLE(cpuCycle) static_cast<size_t>(cpuCycle * 2)
		
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
	
		#undef APU_TO_CPU_CYCLE
	}

private:
	void ClockQuarterFrameChips()
	{
		m_apu->m_pulseChannel0->ClockQuarterFrameChips();
		m_apu->m_pulseChannel1->ClockQuarterFrameChips();
		m_apu->m_triangleChannel->ClockQuarterFrameChips();
		m_apu->m_noiseChannel->ClockQuarterFrameChips();
	}

	void ClockHalfFrameChips()
	{
		m_apu->m_pulseChannel0->ClockHalfFrameChips();
		m_apu->m_pulseChannel1->ClockHalfFrameChips();
		m_apu->m_triangleChannel->ClockHalfFrameChips();
		m_apu->m_noiseChannel->ClockHalfFrameChips();
	}

	Apu* m_apu;
	size_t m_cpuCycles;
	size_t m_numSteps;
	bool m_inhibitInterrupt;
};

void Apu::Initialize()
{
	g_apu = this;

	std::fill(std::begin(m_channelVolumes), std::end(m_channelVolumes), 1.0f);

	m_frameCounter.reset(new FrameCounter(*this));

	m_pulseChannel0 = std::make_shared<PulseChannel>(0);
	m_pulseChannel1 = std::make_shared<PulseChannel>(1);
	m_triangleChannel = std::make_shared<TriangleChannel>();
	m_noiseChannel = std::make_shared<NoiseChannel>();

	m_audioDriver = std::make_shared<AudioDriver>();
	m_audioDriver->Initialize();
}

void Apu::Reset()
{
	m_evenFrame = true;
	m_elapsedCpuCycles = 0;
	m_sampleSum = m_numSamples = 0;
	HandleCpuWrite(0x4017, 0);
	HandleCpuWrite(0x4015, 0);
	for (uint16 address = 0x4000; address <= 0x400F; ++address)
		HandleCpuWrite(address, 0);
}

void Apu::Serialize(class Serializer& serializer)
{
	SERIALIZE(m_evenFrame);
	SERIALIZE(m_elapsedCpuCycles);
	SERIALIZE(m_sampleSum);
	SERIALIZE(m_numSamples);
	SERIALIZE(*m_pulseChannel0);
	SERIALIZE(*m_pulseChannel1);
	SERIALIZE(*m_triangleChannel);
	SERIALIZE(*m_noiseChannel);
	serializer.SerializeObject(*m_frameCounter);
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
			m_triangleChannel->ClockTimer();

			// All other timers are clocked every 2nd CPU cycle (every APU cycle)
			if (m_evenFrame)
			{
				m_pulseChannel0->ClockTimer();
				m_pulseChannel1->ClockTimer();
				m_noiseChannel->ClockTimer();
			}

			m_evenFrame = !m_evenFrame;
		}

	#if SAMPLE_EVERY_CPU_CYCLE
		m_sampleSum += SampleChannelsAndMix();
		++m_numSamples;
	#endif

		// Fill the sample buffer at the current output sample rate (i.e. 48 KHz)
		if (++m_elapsedCpuCycles >= kCpuCyclesPerSample)
		{
			m_elapsedCpuCycles -= kCpuCyclesPerSample;

		#if SAMPLE_EVERY_CPU_CYCLE
			const float32 sample = m_sampleSum / m_numSamples;
			m_sampleSum = m_numSamples = 0;
		#else
			const float32 sample = SampleChannelsAndMix();
		#endif

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

		result.SetPos(0, m_pulseChannel0->GetLengthCounter().GetValue() > 0);
		result.SetPos(1, m_pulseChannel1->GetLengthCounter().GetValue() > 0);
		result.SetPos(2, m_triangleChannel->GetLengthCounter().GetValue() > 0);
		result.SetPos(3, m_noiseChannel->GetLengthCounter().GetValue() > 0);
		break;
	}
	
	return result.Value();
}

void Apu::HandleCpuWrite(uint16 cpuAddress, uint8 value)
{
	switch (cpuAddress)
	{
	case 0x4000:
	case 0x4001:
	case 0x4002:
	case 0x4003:
		m_pulseChannel0->HandleCpuWrite(cpuAddress, value);
		break;

	case 0x4004:
	case 0x4005:
	case 0x4006:
	case 0x4007:
		m_pulseChannel1->HandleCpuWrite(cpuAddress, value);
		break;

	case 0x4008:
	case 0x400A:
	case 0x400B:
		m_triangleChannel->HandleCpuWrite(cpuAddress, value);
		break;

	case 0x400C:
	case 0x400E:
	case 0x400F:
		m_noiseChannel->HandleCpuWrite(cpuAddress, value);
		break;

		/////////////////////
		// Misc
		/////////////////////
	case 0x4015:
		m_pulseChannel0->GetLengthCounter().SetEnabled(TestBits(value, BIT(0)));
		m_pulseChannel1->GetLengthCounter().SetEnabled(TestBits(value, BIT(1)));
		m_triangleChannel->GetLengthCounter().SetEnabled(TestBits(value, BIT(2)));
		m_noiseChannel->GetLengthCounter().SetEnabled(TestBits(value, BIT(3)));
		//@TODO: DMC Enable bit 4
		break;

	case 0x4017:
		m_frameCounter->HandleCpuWrite(cpuAddress, value);
		break;
	}
}

void Apu::SetChannelVolume(ApuChannel::Type type, float32 volume)
{
	m_channelVolumes[type] = Clamp(volume, 0.0f, 1.0f);
}

float32 Apu::SampleChannelsAndMix()
{
	static float kMasterVolume = 1.0f;

	// Sample all channels
	const size_t pulse1 = static_cast<size_t>(m_pulseChannel0->GetValue() * m_channelVolumes[ApuChannel::Pulse1]);
	const size_t pulse2 = static_cast<size_t>(m_pulseChannel1->GetValue() * m_channelVolumes[ApuChannel::Pulse2]);
	const size_t triangle = static_cast<size_t>(m_triangleChannel->GetValue() * m_channelVolumes[ApuChannel::Triangle]);
	const size_t noise = static_cast<size_t>(m_noiseChannel->GetValue() * m_channelVolumes[ApuChannel::Noise]);
	const size_t dmc = static_cast<size_t>(0.0f);

	// Mix samples
#if MIX_USING_LINEAR_APPROXIMATION
	// Linear approximation (less accurate than lookup table)
	const float32 pulseOut = 0.00752f * (pulse1 + pulse2);
	const float32 tndOut = 0.00851f * triangle + 0.00494f * noise + 0.00335f * dmc;
#else
	// Lookup Table (accurate)			
	static float32 pulseTable[31] = { ~0 };
	if (pulseTable[0] == ~0)
	{
		for (size_t i = 0; i < ARRAYSIZE(pulseTable); ++i)
		{
			pulseTable[i] = 95.52f / (8128.0f / i + 100.0f);
		}
	}
	static float32 tndTable[203] = { ~0 };
	if (tndTable[0] == ~0)
	{
		for (size_t i = 0; i < ARRAYSIZE(tndTable); ++i)
		{
			tndTable[i] = 163.67f / (24329.0f / i + 100.0f);
		}
	}

	const float32 pulseOut = pulseTable[pulse1 + pulse2];
	const float32 tndOut = tndTable[3 * triangle + 2 * noise + dmc];
#endif

	const float32 sample = kMasterVolume * (pulseOut + tndOut);
	return sample;
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
	auto pulse = g_apu->m_pulseChannel1.get();

	DrawBar(pulse->GetValue() / 15.0f, Color4::Yellow());
	DrawBar(pulse->m_volumeEnvelope.GetVolume() / 15.0f, Color4::Red());
	DrawBar(pulse->m_volumeEnvelope.m_counter / 15.0f, Color4::Cyan());
	DrawBar(pulse->m_volumeEnvelope.m_constantVolume / 15.0f, Color4::Magenta());
	DrawBar(pulse->m_sweepUnit.SilenceChannel() ? 0.0f : 1.0f, Color4::Green());
	DrawBar(pulse->m_pulseWaveGenerator.GetValue() * 1.0f, Color4::Blue());
	DrawBar(pulse->m_lengthCounter.GetValue() / 255.0f, Color4::Black());

	DrawBar(pulse->m_sweepUnit.m_divider.GetCounter() / 7.0f, Color4::Red());
	DrawBar(pulse->m_timer.m_divider.GetPeriod() / 2047.0f, Color4::Blue());
	DrawBar(pulse->m_timer.m_divider.GetCounter() / 2047.0f, Color4::Green());

	DrawBar(g_apu->m_audioDriver->GetBufferUsageRatio(), Color4::Green());
#endif
}
