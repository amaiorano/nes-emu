#include "AudioDriver.h"
#include "CircularBuffer.h"
#include "Stream.h"

#include <stdexcept>

#define SDL_MAIN_HANDLED // Don't use SDL's main impl
#include <SDL.h>
#include <SDL_audio.h>

#define OUTPUT_RAW_AUDIO_FILE_STREAM 0

template <int type>
struct AudioFormatToType {
	AudioFormatToType() {
		throw std::runtime_error("General format type is not supported. Specialized type should be used");
	}
};

template <> struct AudioFormatToType<AUDIO_S16> {
	typedef int16 Type;
	
	static Type convertFromF32(float32 sample) {
		sample = 2.f * sample - 1.f;
		return static_cast<Type>(sample * std::numeric_limits<int16>::max());
	}
};
template <> struct AudioFormatToType<AUDIO_U16> {
	typedef uint16 Type;
	
	static Type convertFromF32(float32 sample) {
		return static_cast<Type>(sample * std::numeric_limits<int16>::max());
	}
};
template <> struct AudioFormatToType<AUDIO_F32> {
	typedef float32 Type;
	
	static Type convertFromF32(float32 sample) {
		return sample;
	}
};

class AudioDriver::AudioDriverImpl
{
public:
	friend class AudioDriver;

	static const int kSampleRate = 48000;
	static const SDL_AudioFormat kSampleFormat = AUDIO_S16; // Apparently supported by all drivers?
	//static const SDL_AudioFormat kSampleFormat = AUDIO_U16;
	//static const SDL_AudioFormat kSampleFormat = AUDIO_F32;
	static const int kNumChannels = 1;
	static const int kSamplesPerCallback = 1024;

	template <SDL_AudioFormat Format> struct FormatToType;
	
	typedef AudioFormatToType<kSampleFormat>::Type SampleFormatType;

	AudioDriverImpl()
		: m_audioDeviceID(0)
	{
	}

	~AudioDriverImpl()
	{
		Shutdown();
	}

	void Initialize()
	{
		SDL_InitSubSystem(SDL_INIT_AUDIO);
			
		SDL_AudioSpec desired;
		SDL_zero(desired);
		desired.freq = kSampleRate;
		desired.format = kSampleFormat;
		desired.channels = kNumChannels;
		desired.samples = kSamplesPerCallback;
		desired.callback = AudioCallback;
		desired.userdata = this;

		m_audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &desired, NULL/*&m_audioSpec*/, SDL_AUDIO_ALLOW_ANY_CHANGE);
		m_audioSpec = desired;

		if (m_audioDeviceID == 0)
			FAIL("Failed to open audio device (error code %d)", SDL_GetError());

		// Set buffer size as a function of the latency we allow
		const float32 kDesiredLatencySecs = 50 / 1000.0f;
		const float32 desiredLatencySamples = kDesiredLatencySecs * GetSampleRate();
		const size_t bufferSize = static_cast<size_t>(desiredLatencySamples * 2); // We wait until buffer is 50% full to start playing
		m_samples.Init(bufferSize);

	#if OUTPUT_RAW_AUDIO_FILE_STREAM
		m_rawAudioOutputFS.Open("RawAudio.raw", "wb");
	#endif

		SetPaused(true);
	}

	void Shutdown()
	{
		m_rawAudioOutputFS.Close();

		SDL_CloseAudioDevice(m_audioDeviceID);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	size_t GetSampleRate() const
	{
		return m_audioSpec.freq;
	}

	float32 GetBufferUsageRatio() const
	{
		return static_cast<float32>(m_samples.UsedSize()) / m_samples.TotalSize();
	}

	void SetPaused(bool paused)
	{
		if (paused != m_paused)
		{
			m_paused = paused;
			SDL_PauseAudioDevice(m_audioDeviceID, m_paused? 1 : 0);
		}
	}

	void AddSampleF32(float32 sample)
	{
		assert(sample >= 0.0f && sample <= 1.0f);
		auto targetSample = AudioFormatToType<kSampleFormat>::convertFromF32(sample);
		
		AddSampleS16(targetSample);
	}

	void AddSampleS16(int16 targetSample)
	{
		static_assert(kSampleFormat == AUDIO_S16, "only S16 format is supported");
		
		SDL_LockAudioDevice(m_audioDeviceID);
		m_samples.PushBack(static_cast<SampleFormatType>(targetSample));
		SDL_UnlockAudioDevice(m_audioDeviceID);

		// Unpause when buffer is half full; pause if almost depleted to give buffer a chance to
		// fill up again.
		const auto bufferUsageRatio = GetBufferUsageRatio();
		if (bufferUsageRatio >= 0.5f)
		{
			SetPaused(false);
		}
		else if (bufferUsageRatio < 0.1f)
		{
			SetPaused(true);
		}

	#if OUTPUT_RAW_AUDIO_FILE_STREAM
		m_rawAudioOutputFS.WriteValue(sample);
	#endif
	}

private:
	static void AudioCallback(void* userData, Uint8* byteStream, int byteStreamLength)
	{
		auto audioDriver = reinterpret_cast<AudioDriverImpl*>(userData);
		auto stream = reinterpret_cast<SampleFormatType*>(byteStream);

		size_t numSamplesToRead = byteStreamLength / sizeof(SampleFormatType);

		size_t numSamplesRead = audioDriver->m_samples.PopBack(stream, numSamplesToRead);

		// If we haven't written enough samples, fill out the rest with the last sample
		// written. This will usually hide the error.
		if (numSamplesRead < numSamplesToRead)
		{
			SampleFormatType lastSample = numSamplesRead == 0 ? 0 : stream[numSamplesRead - 1];
			std::fill_n(stream + numSamplesRead, numSamplesToRead - numSamplesRead, lastSample);
		}
	}

	SDL_AudioDeviceID m_audioDeviceID;
	SDL_AudioSpec m_audioSpec;
	CircularBuffer<SampleFormatType> m_samples;
	FileStream m_rawAudioOutputFS;
	bool m_paused;
};


AudioDriver::AudioDriver()
	: m_impl(new AudioDriver::AudioDriverImpl)
{
}

AudioDriver::~AudioDriver()
{
	delete m_impl;
}

void AudioDriver::Initialize()
{
	m_impl->Initialize();
}

void AudioDriver::Shutdown()
{
	m_impl->Shutdown();
}

size_t AudioDriver::GetSampleRate() const
{
	return m_impl->GetSampleRate();
}

float32 AudioDriver::GetBufferUsageRatio() const
{
	return m_impl->GetBufferUsageRatio();
}

void AudioDriver::AddSampleF32(float32 sample)
{
	m_impl->AddSampleF32(sample);
}

void AudioDriver::AddSampleS16(int16 sample) {
	m_impl->AddSampleS16(sample);
}
