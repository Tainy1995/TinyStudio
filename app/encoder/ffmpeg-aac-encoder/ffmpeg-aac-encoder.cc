#include "ffmpeg-aac-encoder.h"
#include "core-engine.h"
#include "core-settings.h"
extern "C" {
#include "libavutil/mathematics.h"
}

static inline int64_t rescale_ts(int64_t val, AVCodecContext* context,
	AVRational new_base)
{
	return av_rescale_q_rnd(val, context->time_base, new_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
}

FFMpegAACEncoder::FFMpegAACEncoder()
{
	init_flag_.store(false);
}

FFMpegAACEncoder::~FFMpegAACEncoder()
{
	if (samples[0])
		av_freep(&samples[0]);
	if (context)
		avcodec_close(context);
	if (aframe)
		av_frame_free(&aframe);
}

bool FFMpegAACEncoder::Init()
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	avcodec_register_all();
#endif

	CoreSettings* settings = core_engine_->GetSettings();

	codec = avcodec_find_encoder_by_name("aac");
	if (!codec)
		return false;
	context = avcodec_alloc_context3(codec);
	if (!context)
		return false;

	context->bit_rate = (int64_t)settings->GetOutputParam()->audio.bitrate * 1000;
	context->channels = CoreAudioData::get_audio_channels(settings->GetOutputParam()->audio.speakers);
	context->channel_layout = CoreAudioData::convert_speaker_layout(settings->GetOutputParam()->audio.speakers);
	context->sample_rate = settings->GetOutputParam()->audio.samples_per_sec;
	context->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;

	if (codec->supported_samplerates) {
		const int* rate = codec->supported_samplerates;
		int cur_rate = context->sample_rate;
		int closest = 0;

		while (*rate) {
			int dist = abs(cur_rate - *rate);
			int closest_dist = abs(cur_rate - closest);

			if (dist < closest_dist)
				closest = *rate;
			rate++;
		}

		if (closest)
			context->sample_rate = closest;
	}

	av_opt_set(context->priv_data, "aac_coder", "fast", 0);

	audio_planes = CoreAudioData::get_audio_planes(settings->GetOutputParam()->audio.audio_format, settings->GetOutputParam()->audio.speakers);
	audio_size = CoreAudioData::get_audio_size(settings->GetOutputParam()->audio.audio_format, settings->GetOutputParam()->audio.speakers, 1);

	context->strict_std_compliance = -2;
	context->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

	if (!initialize_codec())
		return false;

	init_flag_.store(true);

	return true;
}

bool FFMpegAACEncoder::initialize_codec()
{
	int ret;

	aframe = av_frame_alloc();
	if (!aframe) {
		return false;
	}

	ret = avcodec_open2(context, codec, NULL);
	if (ret < 0) {
		return false;
	}
	aframe->format = context->sample_fmt;
	aframe->channels = context->channels;
	aframe->channel_layout = context->channel_layout;
	aframe->sample_rate = context->sample_rate;

	frame_size = context->frame_size;
	if (!frame_size)
		frame_size = 1024;

	frame_size_bytes = frame_size * (int)audio_size;

	ret = av_samples_alloc(samples, NULL, context->channels,
		frame_size, context->sample_fmt, 0);
	if (ret < 0) {
		return false;
	}

	return true;
}

bool FFMpegAACEncoder::Update(const char* jsondata)
{
	return true;
}

void FFMpegAACEncoder::InputRawData(uint8_t** data, size_t* lineSize, int plane, uint64_t pts, bool* receiveFrame)
{
	*receiveFrame = false;

	if (!init_flag_.load())
		return;

	AVRational time_base = { 1, context->sample_rate };
	AVPacket avpacket = { 0 };
	int got_packet = 0;
	int ret = 0;

	//ret = avcodec_receive_packet(context, &avpacket);
	//got_packet = (ret == 0);

	//if (got_packet)
	//	goto fillpacket;
	//else
	//{
	//	if (data == nullptr)
	//		return;
	//}

	for (size_t i = 0; i < audio_planes; i++)
		memcpy(samples[i], data[i], frame_size_bytes);

	aframe->nb_samples = frame_size;
	aframe->pts = av_rescale_q(total_samples, time_base, context->time_base);

	ret = avcodec_fill_audio_frame(
		aframe, context->channels, context->sample_fmt,
		samples[0], frame_size_bytes * context->channels, 1);
	if (ret < 0) {
		return;
	}

	total_samples += frame_size;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	ret = avcodec_send_frame(context, aframe);
	if (ret == 0)
		ret = avcodec_receive_packet(context, &avpacket);

	got_packet = (ret == 0);

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		ret = 0;
#else
	ret = avcodec_encode_audio2(enc->context, &avpacket, enc->aframe,
		&got_packet);
#endif
	if (ret < 0) {
		if (ret == AVERROR(EINVAL))
		{
		}
		else if (ret == AVERROR_EOF)
		{

		}
		else if (ret == AVERROR(EAGAIN))
		{

		}
		return;
	}

fillpacket:
	*receiveFrame = !!got_packet;
	if (!got_packet)
		return;


	packet_buffer.resize(avpacket.size);
	memcpy(&packet_buffer[0], avpacket.data, avpacket.size);
	encoded_data_.pts = rescale_ts(avpacket.pts, context, time_base);
	encoded_data_.dts = rescale_ts(avpacket.dts, context, time_base);
	encoded_data_.data = &packet_buffer[0];
	encoded_data_.size = avpacket.size;
	encoded_data_.bAudio = true;
	if (encoded_cnt_ == 0)
	{
		dts_offset_ = encoded_data_.dts;
	}
	encoded_data_.pts -= dts_offset_;
	encoded_cnt_ += 1;
	av_free_packet(&avpacket);
}

void FFMpegAACEncoder::GetEncodedData(BaseEncoder::EncodedFrameData* data)
{
	memcpy(data, &encoded_data_, sizeof(encoded_data_));
}

void FFMpegAACEncoder::GetEncoderExtraData(uint8_t** data, size_t* size)
{

}
