#ifndef AUDIO_RESAMPLER_H
#define AUDIO_RESAMPLER_H

#include <vector>
#include <string>
#include "core-audio-data.h"

class AudioResampler
{
public:
	AudioResampler();
	~AudioResampler();
	bool CreateAudioResampler(const CoreAudioData::resample_info* from, const CoreAudioData::resample_info* to);
	bool Resampler(uint8_t* output[], uint32_t* out_frames,
		uint64_t* ts_offset,
		const uint8_t* const input[],
		uint32_t in_frames);

private:
	struct SwrContext* context = nullptr;
	bool opened = false;
	uint32_t input_freq = 0;
	uint64_t input_layout = 0;
	enum AVSampleFormat input_format;

	uint8_t* output_buffer[MAX_AV_PLANES] = { 0 };
	uint64_t output_layout = 0;
	enum AVSampleFormat output_format;
	int output_size = 0;
	uint32_t output_ch = 0;
	uint32_t output_freq = 0;
	uint32_t output_planes = 0;
};

#endif