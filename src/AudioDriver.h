#pragma once
#include "Base.h"

class AudioDriver
{
public:
	AudioDriver();
	~AudioDriver();

	void Initialize();
	void Shutdown();

	size_t GetSampleRate() const;
	float32 GetBufferUsageRatio() const;

	void AddSampleF32(float32 sample);

private:
	class AudioDriverImpl;
	AudioDriverImpl* m_impl;
};
