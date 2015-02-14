#include "AudioDriver.h"
#include "CircularBuffer.h"
#define SDL_MAIN_HANDLED // Don't use SDL's main impl
#include <SDL.h>
#include <SDL_audio.h>

class AudioDriver::AudioDriverImpl
{
public:
	static const int kSampleRate = 48000;
	//static const SDL_AudioFormat kSampleFormat = AUDIO_S16; // Apparently supported by all drivers?
	static const SDL_AudioFormat kSampleFormat = AUDIO_U16;
	static const int kNumChannels = 1;
	static const int kSamplesPerCallback = 1024;

	template <SDL_AudioFormat Format> struct FormatToType;
	template <> struct FormatToType<AUDIO_S16> { typedef int16 Type; };
	template <> struct FormatToType<AUDIO_U16> { typedef uint16 Type; };

	typedef FormatToType<kSampleFormat>::Type SampleFormatType;

	AudioDriverImpl()
		: m_audioDevice(0)
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

		m_audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, NULL/*&m_audioSpec*/, /*SDL_AUDIO_ALLOW_FORMAT_CHANGE*/0);
		m_audioSpec = desired;

		if (m_audioDevice == 0)
			FAIL("Failed to open audio device (error code %d)", SDL_GetError());

		// The minimize audio delay in the case where we write faster than the audio thread can read, we
		// must make the buffer as small as possible - that is, twice the number of samples we need to feed
		// the audio device per callback.
		const size_t bufferSize = kSamplesPerCallback * 2;
		m_samples.Init(bufferSize);

		SDL_PauseAudioDevice(m_audioDevice, 0); // Unpause audio
	}

	void Shutdown()
	{
		SDL_CloseAudioDevice(m_audioDevice);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	void AddSampleF32(float32 sample)
	{
		assert(sample >= 0.0f && sample <= 1.0f);
		//@TODO: This multiply is wrong for signed format types (S16, S32)
		float targetSample = sample * std::numeric_limits<SampleFormatType>::max();
		m_samples.Write(static_cast<SampleFormatType>(targetSample));
	}

private:
	static void AudioCallback(void* userData, Uint8* byteStream, int byteStreamLength)
	{
		auto audioDriver = reinterpret_cast<AudioDriverImpl*>(userData);
		auto stream = reinterpret_cast<SampleFormatType*>(byteStream);

		size_t numSamplesToRead = byteStreamLength / sizeof(SampleFormatType);

		size_t numSamplesRead = audioDriver->m_samples.Read(stream, numSamplesToRead);

		if (numSamplesRead < numSamplesToRead)
		{
			std::fill_n(stream + numSamplesRead, numSamplesToRead - numSamplesRead, 0);
		}
	}

	SDL_AudioDeviceID m_audioDevice;
	SDL_AudioSpec m_audioSpec;
	CircularBuffer<SampleFormatType> m_samples;
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
	return AudioDriverImpl::kSampleRate;
}

void AudioDriver::AddSampleF32(float32 sample)
{
	m_impl->AddSampleF32(sample);
}
