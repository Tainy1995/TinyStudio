#include "media-controler.h"
#include <Windows.h>
#include <codecvt>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstdio>
#include "platform.h"
#include "logger.h"

#define MAX_AUDIO_FRAME_SIZE 4096

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
#define USE_NEW_FFMPEG_DECODE_API
#endif

static inline int get_sws_colorspace(enum AVColorSpace cs)
{
	switch (cs) {
	case AVCOL_SPC_BT709:
		return SWS_CS_ITU709;
	case AVCOL_SPC_FCC:
		return SWS_CS_FCC;
	case AVCOL_SPC_SMPTE170M:
		return SWS_CS_SMPTE170M;
	case AVCOL_SPC_SMPTE240M:
		return SWS_CS_SMPTE240M;
	default:
		break;
	}

	return SWS_CS_ITU601;
}
#define FIXED_1_0 (1 << 16)

static inline int get_sws_range(enum AVColorRange r)
{
	return r == AVCOL_RANGE_JPEG ? 1 : 0;
}

void circle_clear (struct circlebuf* d)
{
	while (d->size) {
		AVPacket pkt;
		circlebuf_pop_front(d, &pkt, sizeof(AVPacket));
		av_packet_unref(&pkt);
	}
}

MediaControler::MediaControler(MediaStopped cb)
{

	static bool initialized = false;
	if (!initialized) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		av_register_all();
		avcodec_register_all();
#endif
		avdevice_register_all();
		avformat_network_init();
		initialized = true;
	}

	media_stopped_cb_ = cb;
	media_started_.store(false);
	video_cnt_.store(0);
	audio_cnt_.store(0);
	frame_ready_.store(false);
	audio_ready_.store(false);

	circlebuf_init(&video_packets_);
	circlebuf_init(&audio_packets_);

	audio_monitor_ = new WasapiAudioMonitor();
}

MediaControler::~MediaControler()
{
	media_started_.store(false);
	if (media_thread_.joinable())
		media_thread_.join();
	if (audio_in_frame)
	{
		av_frame_unref(audio_in_frame);
		av_free(audio_in_frame);
	}
	if (in_frame) {
		av_frame_unref(in_frame);
		av_free(in_frame);
	}
	if (decoder_) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		avcodec_free_context(&decoder_);
#else
		avcodec_close(decoder_);
#endif
	}
	if (audio_decoder_) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		avcodec_free_context(&audio_decoder_);
#else
		avcodec_close(audio_decoder_);
#endif
	}
	if (audio_monitor_)
	{
		delete audio_monitor_;
		audio_monitor_ = nullptr;
	}
	sws_freeContext(swscale_context_);
	if(scale_pic[0])
		av_freep(&scale_pic[0]);
	circle_clear(&video_packets_);
	circle_clear(&audio_packets_);
	circlebuf_free(&video_packets_);
	circlebuf_free(&audio_packets_);

	if(av_format_context_)
		avformat_close_input(&av_format_context_);
}

bool MediaControler::StartMedia(const wchar_t* filepath)
{
	if (media_thread_.joinable())
		media_thread_.join();
	media_file_name_ = filepath;
	media_started_.store(true);
	media_thread_ = std::thread([this]() {
		MediaThreadImpl();
		});
	return true;
}

void MediaControler::MediaThreadImpl()
{

	if (!InitAvFormatContext())
		goto end;

	if (!InitDecoder())
		goto end;

	if (!InitAudioDecoder())
		goto end;

	start_pts_ = os_gettime_ns();

	while (1)
	{
		if (!media_started_.load())
			break;

		RenderAudio();

		bool get_frame = false;
		bool error = false;
		while (!get_frame && media_started_.load())
		{
			if (frame_ready_.load())
			{
				get_frame = true;
				break;
			}
			if (!DecodeNext(get_frame))
			{
				error = true;
				break;
			}
			if (get_frame)
				break;
		}

		if (error)
			break;

		get_frame = false;
		while (!get_frame && media_started_.load())
		{
			if (audio_ready_.load())
			{
				get_frame = true;
				break;
			}
			if (!DecodeNextAudio(get_frame))
			{
				error = true;
				break;
			}
			if (get_frame)
				break;
		}
		if (error)
			break;

		if (!media_started_.load())
			break;

		int64_t minTime = video_time_.next_pts;
		if (minTime > audio_time_.next_pts)
			minTime = audio_time_.next_pts;

		int64_t curTime = os_gettime_ns();
		if((start_pts_ + minTime) > curTime)
			std::this_thread::sleep_for(std::chrono::nanoseconds( (start_pts_ + minTime) - curTime));
	}

end:
	if (media_stopped_cb_)
		media_stopped_cb_();
}

void MediaControler::CalcAudioFramePts()
{
	int64_t last_pts = audio_time_.frame_pts;

	if (audio_in_frame->best_effort_timestamp == AV_NOPTS_VALUE)
		audio_time_.frame_pts = audio_time_.next_pts;
	else
		audio_time_.frame_pts = av_rescale_q(audio_in_frame->best_effort_timestamp, audio_decoder_->time_base,
			(AVRational)av_make_q(1, 1000000000));

	int64_t duration = audio_in_frame->pkt_duration;
	if (!duration)
		duration = GetEstimatedDuration(last_pts, true);
	else
		duration = av_rescale_q(duration, audio_decoder_->time_base,
			(AVRational)av_make_q(1, 1000000000));

	audio_time_.last_duration = duration;
	audio_time_.next_pts = audio_time_.frame_pts + duration;
}

void MediaControler::CalcFramePts()
{
	int64_t last_pts = video_time_.frame_pts;

	if (in_frame->best_effort_timestamp == AV_NOPTS_VALUE)
		video_time_.frame_pts = video_time_.next_pts;
	else
		video_time_.frame_pts = av_rescale_q(in_frame->best_effort_timestamp, video_stream_->time_base,
			(AVRational)av_make_q(1, 1000000000));

	int64_t duration = in_frame->pkt_duration;
	if (!duration)
		duration = GetEstimatedDuration(last_pts, false);
	else
		duration = av_rescale_q(duration, video_stream_->time_base,
			(AVRational)av_make_q(1, 1000000000));

	video_time_.last_duration = duration;
	video_time_.next_pts = video_time_.frame_pts + duration;
}

int64_t MediaControler::GetEstimatedDuration(int64_t last_pts, bool bAudio)
{
	if (!bAudio)
	{
		if (last_pts)
			return video_time_.frame_pts - last_pts;

		if (video_time_.last_duration)
			return video_time_.last_duration;
		return av_rescale_q(video_stream_->time_base.num,
			video_stream_->time_base, (AVRational)av_make_q(1, 1000000000));
	}
	else
	{
		if (last_pts)
			return audio_time_.frame_pts - last_pts;

		if (audio_time_.last_duration)
			return audio_time_.last_duration;

		return av_rescale_q(audio_in_frame->nb_samples,
			(AVRational)av_make_q (1, audio_in_frame->sample_rate),
			(AVRational)av_make_q (1, 1000000000));
	}
}

void MediaControler::GetFrameData(uint8_t** pic, int* lineSize, int& width, int& height)
{
	for (int i = 0; i < 4; i++)
	{
		pic[i] = scale_pic[i];
		lineSize[i] = scale_linesizes[i];
	}
	width = decoder_->width;
	height = decoder_->height;
}

void MediaControler::RenderFrameFinish()
{
	if (audio_monitor_)
		audio_monitor_->UpdateLastVideoFrameTime(video_time_.frame_pts);

	video_cnt_.fetch_add(1);
	frame_ready_.store(false);
}

void MediaControler::RenderAudio()
{
	if (audio_ready_.load())
	{
		if ((audio_time_.frame_pts + start_pts_) <= os_gettime_ns())
		{
			if (audio_monitor_)
			{
				audio_monitor_->InputAudioData(audio_in_frame->data, audio_in_frame->nb_samples, audio_time_.frame_pts);
			}
			audio_ready_.store(false);
		}
	}
}

bool MediaControler::GetFrameReady()
{
	if (frame_ready_.load())
	{
		if ((start_pts_ + video_time_.frame_pts) < os_gettime_ns() && video_time_.frame_pts <= audio_time_.frame_pts)
		{
			return frame_ready_.load();
		}
		else
		{
			return false;
		}
	}
	return false;
}

bool MediaControler::ScaleVideoFrame()
{
	int ret = sws_scale(swscale_context_, (const uint8_t* const*)in_frame->data,
		in_frame->linesize, 0, in_frame->height, scale_pic, scale_linesizes);
	if (ret < 0)
		return false;
	return true;
}

bool MediaControler::InitAudioScaling()
{
	if (audio_monitor_)
	{
		CoreAudioData::resample_info info;
		info.format = CoreAudioData::convert_sample_format(audio_decoder_->sample_fmt);
		info.samples_per_sec = audio_decoder_->sample_rate;
		info.speakers = CoreAudioData::convert_speaker_layout(audio_decoder_->channels);
		return audio_monitor_->CreateWasapiMonitor(&info);
	}
	return true;
}

bool MediaControler::InitScaling()
{
	int space = get_sws_colorspace(decoder_->colorspace);
	int range = get_sws_range(decoder_->color_range);
	const int* coeff = sws_getCoefficients(space);

	swscale_context_ = sws_getCachedContext(NULL, decoder_->width,
		decoder_->height,
		decoder_->pix_fmt,
		decoder_->width,
		decoder_->height, AV_PIX_FMT_BGRA,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (!swscale_context_) {
		return false;
	}

	sws_setColorspaceDetails(swscale_context_, coeff, range, coeff, range, 0,
		FIXED_1_0, FIXED_1_0);

	int ret = av_image_alloc(scale_pic, scale_linesizes,
		decoder_->width, decoder_->height,
		AV_PIX_FMT_BGRA, 1);

	if (ret < 0) {
		return false;
	}

	return true;
}

bool MediaControler::DecodeNextAudio(bool& get_frame)
{
	if (audio_packets_.data == NULL || audio_packets_.size <= 0)
	{
		ReadFromFile(false);
	}

	if (audio_packets_.data && audio_packets_.size > 0)
	{
		int ret = 0;
		AVPacket pkt;
		circlebuf_pop_front(&audio_packets_, &pkt, sizeof(pkt));

		get_frame = false;
		ret = avcodec_receive_frame(audio_decoder_, audio_in_frame);
		if (ret != 0)
		{
			ret = avcodec_send_packet(audio_decoder_, &pkt);
			if (ret == 0)
				ret = avcodec_receive_frame(audio_decoder_, audio_in_frame);
		}

		get_frame = (ret == 0);
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			ret = 0;

		av_packet_unref(&pkt);

		if (get_frame)
		{
			CalcAudioFramePts();
			if (!InitAudioScaling())
				return false;
			audio_ready_.store(true);
		}

		if (ret < 0)
			return false;
		else
			return true;
	}

	return true;
}

bool MediaControler::ReadFromFile(bool bVideo)
{
	while (1)
	{
		AVPacket pkt;
		AVPacket new_pkt;
		av_init_packet(&pkt);
		new_pkt = pkt;

		int ret = av_read_frame(av_format_context_, &pkt);
		if (ret < 0)
		{
			std::cout << "read error ret:" << ret << std::endl;
			return false;
		}

		if (pkt.stream_index == audio_stream_->index)
		{
			av_packet_ref(&new_pkt, &pkt);
			circlebuf_push_back(&audio_packets_, &new_pkt, sizeof(AVPacket));
			if (!bVideo)
			{
				av_packet_unref(&pkt);
				return true;
			}
		}
		else if (pkt.stream_index == video_stream_->index)
		{
			av_packet_ref(&new_pkt, &pkt);
			circlebuf_push_back(&video_packets_, &new_pkt, sizeof(AVPacket));
			if (bVideo)
			{
				av_packet_unref(&pkt);
				return true;
			}
		}

		av_packet_unref(&pkt);
	}

}

bool MediaControler::DecodeNext(bool& get_frame)
{
	if (video_packets_.data == NULL || video_packets_.size <= 0)
	{
		ReadFromFile(true);
	}

	if (video_packets_.data && video_packets_.size > 0)
	{
		int ret = 0;
		AVPacket pkt;
		circlebuf_pop_front(&video_packets_, &pkt, sizeof(pkt));

		get_frame = false;
		ret = avcodec_receive_frame(decoder_, in_frame);
		if (ret != 0)
		{
			ret = avcodec_send_packet(decoder_, &pkt);
			if (ret == 0)
				ret = avcodec_receive_frame(decoder_, in_frame);
		}

		get_frame = (ret == 0);
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			ret = 0;

		av_packet_unref(&pkt);

		if (get_frame)
		{
			CalcFramePts();
			if (!swscale_context_)
			{
				if (!InitScaling())
					return false;
			}

			if (!ScaleVideoFrame())
				return false;

			frame_ready_.store(true);
		}

		if (ret < 0)
			return false;
		else
			return true;
	}

	return true;
}

bool MediaControler::InitAvFormatContext()
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
	av_format_context_ = avformat_alloc_context();
	int ret = avformat_open_input(&av_format_context_, cv.to_bytes(media_file_name_).c_str(), nullptr, nullptr);
	if (ret < 0) 
		return false;
	if (avformat_find_stream_info(av_format_context_, nullptr) < 0) {
		return false;
	}
	return true;
}

bool MediaControler::InitAudioDecoder()
{
	enum AVCodecID id;
	int ret;
	ret = av_find_best_stream(av_format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (ret < 0)
		return false;
	audio_stream_ = av_format_context_->streams[ret];
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	id = audio_stream_->codecpar->codec_id;
#else
	id = audio_stream_->codec->codec_id;
#endif

	if (!audio_av_codec_)
		audio_av_codec_ = avcodec_find_decoder(id);

	if (!audio_av_codec_)
		return false;

	audio_in_frame = av_frame_alloc();
	if (!audio_in_frame)
	{
		return false;
	}
	AVCodecContext* c;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	c = avcodec_alloc_context3(audio_av_codec_);
	if (!c) {
		return false;
	}

	ret = avcodec_parameters_to_context(c, audio_stream_->codecpar);
	if (ret < 0)
		goto fail;
#else
	c = d->stream->codec;
#endif

	if (c->thread_count == 1 && c->codec_id != AV_CODEC_ID_PNG &&
		c->codec_id != AV_CODEC_ID_TIFF &&
		c->codec_id != AV_CODEC_ID_JPEG2000 &&
		c->codec_id != AV_CODEC_ID_MPEG4 && c->codec_id != AV_CODEC_ID_WEBP)
		c->thread_count = 0;

	ret = avcodec_open2(c, audio_av_codec_, NULL);
	if (ret < 0)
		goto fail;

	audio_decoder_ = c;
	return ret == 0 ? true : false;

fail:
	avcodec_close(c);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	av_free(audio_decoder_);
#endif
	return false;
}

bool MediaControler::InitDecoder()
{
	enum AVCodecID id;
	int ret;
	ret = av_find_best_stream(av_format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (ret < 0)
		return false;
	video_stream_ = av_format_context_->streams[ret];
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	id = video_stream_->codecpar->codec_id;
#else
	id = video_stream_->codec->codec_id;
#endif

	if (id == AV_CODEC_ID_VP8 || id == AV_CODEC_ID_VP9) {
		AVDictionaryEntry* tag = NULL;
		tag = av_dict_get(video_stream_->metadata, "alpha_mode", tag,
			AV_DICT_IGNORE_SUFFIX);

		if (tag && strcmp(tag->value, "1") == 0) {
			char* codec = (id == AV_CODEC_ID_VP8) ? "libvpx"
				: "libvpx-vp9";
			av_codec_ = avcodec_find_decoder_by_name(codec);
		}
	}

	if (!av_codec_)
		av_codec_ = avcodec_find_decoder(id);

	if (!av_codec_) 
		return false;
	in_frame = av_frame_alloc();
	if (!in_frame)
	{
		return false;
	}
	AVCodecContext* c;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	c = avcodec_alloc_context3(av_codec_);
	if (!c) {
		return false;
	}

	ret = avcodec_parameters_to_context(c, video_stream_->codecpar);
	if (ret < 0)
		goto fail;
#else
	c = d->stream->codec;
#endif

	if (c->thread_count == 1 && c->codec_id != AV_CODEC_ID_PNG &&
		c->codec_id != AV_CODEC_ID_TIFF &&
		c->codec_id != AV_CODEC_ID_JPEG2000 &&
		c->codec_id != AV_CODEC_ID_MPEG4 && c->codec_id != AV_CODEC_ID_WEBP)
		c->thread_count = 0;

	ret = avcodec_open2(c, av_codec_, NULL);
	if (ret < 0)
		goto fail;

	decoder_ = c;
	return ret == 0 ? true : false;

fail:
	avcodec_close(c);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	av_free(decoder_);
#endif
	return false;
}
