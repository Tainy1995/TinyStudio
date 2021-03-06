#include "audio-resampler.h"

AudioResampler::AudioResampler()
{

}

AudioResampler::~AudioResampler()
{
	if (context)
		swr_free(&context);
	if (output_buffer[0])
		av_freep(&output_buffer[0]);
}

bool AudioResampler::CreateAudioResampler(const CoreAudioData::resample_info* from, const CoreAudioData::resample_info* to)
{
	int errcode = 0;

	opened = false;
	input_freq = from->samples_per_sec;
	input_layout = CoreAudioData::convert_speaker_layout(from->speakers);
	input_format = CoreAudioData::convert_audio_format(from->format);
	output_size = 0;
	output_ch = CoreAudioData::get_audio_channels(to->speakers);
	output_freq = to->samples_per_sec;
	output_layout = CoreAudioData::convert_speaker_layout(to->speakers);
	output_format = CoreAudioData::convert_audio_format(to->format);
	output_planes = CoreAudioData::is_audio_planar(to->format) ? output_ch : 1;

	context = swr_alloc_set_opts(NULL, output_layout,
		output_format,
		to->samples_per_sec, input_layout,
		input_format, from->samples_per_sec,
		0, NULL);

	if (!context) {
		return false;
	}

	if (input_layout == AV_CH_LAYOUT_MONO && output_ch > 1) {
		const double matrix[MAX_AUDIO_CHANNELS][MAX_AUDIO_CHANNELS] = {
			{1},
			{1, 1},
			{1, 1, 0},
			{1, 1, 1, 1},
			{1, 1, 1, 0, 1},
			{1, 1, 1, 1, 1, 1},
			{1, 1, 1, 0, 1, 1, 1},
			{1, 1, 1, 0, 1, 1, 1, 1},
		};
		if (swr_set_matrix(context, matrix[output_ch - 1], 1) < 0)
		{

		}
	}

	errcode = swr_init(context);
	if (errcode != 0) {
		return false;
	}
	return true;
}

bool AudioResampler::Resampler(uint8_t* output[], uint32_t* out_frames,
	uint64_t* ts_offset,
	const uint8_t* const input[],
	uint32_t in_frames)
{

	int ret;
	int64_t delay = swr_get_delay(context, input_freq);
	int estimated = (int)av_rescale_rnd(delay + (int64_t)in_frames,
		(int64_t)output_freq,
		(int64_t)input_freq,
		AV_ROUND_UP);

	*ts_offset = (uint64_t)swr_get_delay(context, 1000000000);

	if (estimated > output_size) {
		if (output_buffer[0])
			av_freep(&output_buffer[0]);

		av_samples_alloc(output_buffer, NULL, output_ch,
			estimated, output_format, 0);

		output_size = estimated;
	}

	ret = swr_convert(context, output_buffer, output_size,
		(const uint8_t**)input, in_frames);

	if (ret < 0) {
		return false;
	}

	for (uint32_t i = 0; i < output_planes; i++)
		output[i] = output_buffer[i];

	*out_frames = (uint32_t)ret;
	return true;
}