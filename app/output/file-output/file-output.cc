#include "file-output.h"
#include "logger.h"
#include "core-settings.h"
#include "core-engine.h"

extern "C" {
#include "libavutil/log.h"
#include "libavutil/mathematics.h"
#include "libavdevice/avdevice.h"
}

static void ffmpeg_log_callback(void* param, int level, const char* format, va_list args)
{
	if (level <= AV_LOG_INFO)
		LOGGER_INFO(format, args);
}

FileOutput::FileOutput()
{
	circlebuf_init(&av_packets_);
}

FileOutput::~FileOutput()
{
	StopOutputImpl();

	if (video_encoder_)
		video_encoder_.reset();
	if (audio_encoder_)
		audio_encoder_.reset();
	circlebuf_free(&av_packets_);
}

void FileOutput::SetVideoEncoder(std::shared_ptr<IBaseEncoder> encoder)
{
	video_encoder_ = encoder;
}

void FileOutput::SetAudioEncoder(std::shared_ptr<IBaseEncoder> encoder)
{
	audio_encoder_ = encoder;
}

bool FileOutput::Update(const char* json)
{
	return true;
}

bool FileOutput::Init()
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
	CoreSettings* settings = core_engine_->GetSettings();
	AVOutputFormat* output_format = av_guess_format(nullptr, settings->GetOutputParam()->path, nullptr);
	if (!output_format) {
		LOGGER_ERROR("sUrl:%s av_guess_format failed", settings->GetOutputParam()->path);
		goto startFail;
	}
	avformat_alloc_output_context2(&av_format_context_, output_format, nullptr, settings->GetOutputParam()->path);
	if (!av_format_context_) {
		LOGGER_ERROR("avformat_alloc_output_context2 failed");
		goto startFail;
	}

	av_format_context_->oformat->video_codec = AVCodecID::AV_CODEC_ID_H264;
	av_format_context_->oformat->audio_codec = AVCodecID::AV_CODEC_ID_AAC;

	if (!CreateVideoStream()) {
		LOGGER_ERROR("CreateVideoStream failed");
		goto startFail;
	}

	if (!CreateAudioStream())
	{
		LOGGER_ERROR("CreateAudioStream failed");
		goto startFail;
	}

	if (!OpenOutputFile()) {
		LOGGER_ERROR("OpenOutputFile failed");
		goto startFail;
	}

	output_released_ = false;
	output_running_ = true;
	output_start_flag_ = true;
	output_run_thread_ = new std::thread(&FileOutput::SendOutputThreadImpl, this);
	return true;

startFail:
	output_start_flag_ = false;
	return false;
}

void FileOutput::StopOutputImpl()
{
	bool exitThread = false;
	{
		std::unique_lock<std::mutex> lock(output_mutex_);
		if (output_released_)
			return;
		if (output_running_)
		{
			exitThread = true;
			output_running_ = false;
		}
	}

	if (exitThread)
	{
		output_cond_.notify_one();
		if (output_run_thread_) {
			if (output_run_thread_->joinable())
				output_run_thread_->join();
			delete output_run_thread_;
			output_run_thread_ = nullptr;
		}
	}

	std::unique_lock<std::mutex> lock(output_mutex_);
	FreeCirclePackets();
	av_write_trailer(av_format_context_);
	if(av_video_codec_context_)
		avcodec_close(av_video_codec_context_);
	av_frame_unref(av_video_frame_);
	av_frame_free(&av_video_frame_);
	if (av_audio_codec_context_)
		avcodec_close(av_audio_codec_context_);
	if (av_audio_streams_)
	{
		for (int idx = 0; idx < 1; idx++) {

			//if (av_audio_streams_[idx])
			//	avcodec_close(av_audio_streams_[idx]->codec);
			if (av_audio_frame_[idx])
				av_frame_free(&av_audio_frame_[idx]);
		}
		free(av_audio_streams_);
		av_audio_streams_ = nullptr;
	}
	if (av_format_context_) {
		if ((av_format_context_->oformat->flags & AVFMT_NOFILE) == 0)
			avio_close(av_format_context_->pb);
		avformat_free_context(av_format_context_);
	}
	output_released_ = true;
}

void FileOutput::InputEncodedData(BaseEncoder::EncodedFrameData encoded_data)
{
	std::unique_lock<std::mutex> lock(output_mutex_);

	if (!output_running_)
		return;
	struct BaseEncoder::EncodedFrameData new_packet;
	memset(&new_packet, 0, sizeof(new_packet));
	new_packet.size = encoded_data.size;
	new_packet.pts = encoded_data.pts;
	new_packet.dts = encoded_data.dts;
	new_packet.keyframe = encoded_data.keyframe;
	new_packet.data = (uint8_t*)malloc(new_packet.size);
	new_packet.bAudio = encoded_data.bAudio;
	memset(new_packet.data, 0, new_packet.size);
	memcpy(new_packet.data, encoded_data.data, new_packet.size);
	circlebuf_push_back(&av_packets_, &new_packet, sizeof(struct BaseEncoder::EncodedFrameData));

	//LOGGER_INFO("%s pts:%lld dts:%lld", new_packet.bAudio ? "audio" : "video", new_packet.pts, new_packet.dts);

	output_cond_.notify_one();
}

bool FileOutput::AllocNewAvAudioStream(AVStream** stream, AVCodec** codec)
{
	*codec = avcodec_find_encoder(AVCodecID::AV_CODEC_ID_AAC);

	if (!*codec) {
		LOGGER_ERROR("avcodec_find_encoder find AV_CODEC_ID_AAC failed");
		return false;
	}

	*stream = avformat_new_stream(av_format_context_, *codec);
	if (!*stream) {
		LOGGER_ERROR("avformat_new_stream failed");
		return false;
	}
	(*stream)->id = av_format_context_->nb_streams - 1;
	LOGGER_INFO(" avstreamid:%d nb_streams:%d", (*stream)->id, av_format_context_->nb_streams);
	return true;
}

bool FileOutput::OpenAudioCodec() {
	//AVCodecContext* context = av_audio_streams_[0]->codec;
	AVCodecContext* context = av_audio_codec_context_;
	int ret = 0;
	av_audio_frame_[0] = av_frame_alloc();
	if (!av_audio_frame_[0]) {
		LOGGER_ERROR("av_frame_alloc av_audio_frame_ failed");
		return false;
	}

	av_audio_frame_[0]->format = context->sample_fmt;
	av_audio_frame_[0]->channels = context->channels;
	av_audio_frame_[0]->channel_layout = context->channel_layout;
	av_audio_frame_[0]->sample_rate = context->sample_rate;

	context->strict_std_compliance = -2;

	ret = avcodec_open2(context, av_audio_codec_, NULL);
	if (ret < 0) {
		LOGGER_ERROR("avcodec_open2 failed");
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_parameters_from_context(av_audio_streams_[0]->codecpar, context);
#endif

	return true;
}

bool FileOutput::CreateNewAudioStream()
{
	CoreSettings* settings = core_engine_->GetSettings();
	AVCodecContext* context;
	AVStream* stream;
	LOGGER_INFO("speakers:%d samples_per_sec:%d bitrate:%d", settings->GetOutputParam()->audio.speakers,
		settings->GetOutputParam()->audio.samples_per_sec, settings->GetOutputParam()->audio.bitrate);

	if (!AllocNewAvAudioStream(&stream, &av_audio_codec_)) {
		LOGGER_ERROR("AllocNewAvAudioStream failed");
		return false;
	}

	av_audio_streams_[0] = stream;
	//context = av_audio_streams_[0]->codec;
	context = avcodec_alloc_context3(av_audio_codec_);
	context->bit_rate = (int64_t)settings->GetOutputParam()->audio.bitrate * 1000;
	context->time_base = (AVRational)av_make_q(1, settings->GetOutputParam()->audio.samples_per_sec);
	context->channels = CoreAudioData::get_audio_channels(settings->GetOutputParam()->audio.speakers);
	context->sample_rate = settings->GetOutputParam()->audio.samples_per_sec;
	context->channel_layout = av_get_default_channel_layout(context->channels);

	//AVlib default channel layout for 5 channels is 5.0 ; fix for 4.1
	if (settings->GetAudioParam()->speakers == CoreAudioData::SPEAKERS_4POINT1)
		context->channel_layout = av_get_channel_layout("4.1");

	context->sample_fmt = av_audio_codec_->sample_fmts
		? av_audio_codec_->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;

	stream->time_base = context->time_base;

	if (av_format_context_->oformat->flags & AVFMT_GLOBALHEADER)
		context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	av_audio_codec_context_ = context;

	return OpenAudioCodec();
}

bool FileOutput::CreateAudioStream() {
	av_audio_streams_ = (AVStream**)calloc(1, sizeof(void*));
	if (!CreateNewAudioStream())
		return false;
	return true;
}

bool FileOutput::CreateVideoStream()
{
	CoreSettings* settings = core_engine_->GetSettings();
	LOGGER_INFO("width:%d height:%d bitrate:%d fps:%d", settings->GetOutputParam()->video.width,
		settings->GetOutputParam()->video.height, settings->GetOutputParam()->video.bitrate, settings->GetOutputParam()->video.fps);
	if (!video_encoder_)
		return false;
	int ret = 0;
	enum AVPixelFormat closest_format;
	enum AVPixelFormat video_format = AV_PIX_FMT_YUV420P;
	enum AVColorRange color_range = AVCOL_RANGE_JPEG;
	enum AVColorSpace color_space = AVCOL_SPC_BT709;
	AVCodecContext* context;

	if (!AllocNewStream()) {
		LOGGER_ERROR("AllocNewStream failed");
		return false;
	}

	closest_format = avcodec_find_best_pix_fmt_of_list(av_video_codec_->pix_fmts, video_format, 0, nullptr);

	//context = av_video_stream_->codec;
	context = avcodec_alloc_context3(av_video_codec_);
	context->bit_rate = (int64_t)settings->GetOutputParam()->video.bitrate;
	context->width = settings->GetOutputParam()->video.width;
	context->height = settings->GetOutputParam()->video.height;
	context->coded_width = settings->GetOutputParam()->video.width;
	context->coded_height = settings->GetOutputParam()->video.height;
	context->time_base = (AVRational)av_make_q(1, settings->GetOutputParam()->video.fps);
	//context->gop_size = 120;
	context->pix_fmt = closest_format;
	//context->thread_count = 0;
	context->color_range = color_range;
	context->colorspace = color_space;

	av_video_stream_->time_base = context->time_base;

#if LIBAVFORMAT_VERSION_MAJOR < 59
	// codec->time_base may still be used if LIBAVFORMAT_VERSION_MAJOR < 59
	av_video_stream_->codec->time_base = context->time_base;
#endif
	av_video_stream_->avg_frame_rate = av_inv_q(context->time_base);

	if (av_format_context_->oformat->flags & AVFMT_GLOBALHEADER)
		context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	av_video_codec_context_ = context;

	ret = avcodec_open2(context, av_video_codec_, nullptr);
	if (ret < 0) {
		LOGGER_ERROR("avcodec_open2 failed");
		return false;
	}
	av_video_frame_ = av_frame_alloc();

	if (!av_video_frame_) {
		LOGGER_ERROR("av_frame_alloc failed frame empty");
		return false;
	}

	av_video_frame_->format = context->pix_fmt;
	av_video_frame_->width = context->width;
	av_video_frame_->height = context->height;
	av_video_frame_->colorspace = color_space;
	av_video_frame_->color_range = color_range;

	ret = av_frame_get_buffer(av_video_frame_, ALIGNMENT);
	if (ret < 0) {
		LOGGER_ERROR("av_frame_get_buffer failed");
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_parameters_from_context(av_video_stream_->codecpar, context);
#endif

	return true;
}

bool FileOutput::AllocNewStream()
{
	av_video_codec_ = avcodec_find_encoder(AVCodecID::AV_CODEC_ID_H264);

	if (!av_video_codec_) {
		LOGGER_ERROR("avcodec_find_encoder failed");
		return false;
	}

	av_video_stream_ = avformat_new_stream(av_format_context_, av_video_codec_);
	if (!av_video_stream_) {
		LOGGER_ERROR("avformat_new_stream failed");
		return false;
	}
	LOGGER_INFO(" avstreamid:%d nb_streams:%d", av_video_stream_->id, av_format_context_->nb_streams);
	av_video_stream_->id = av_format_context_->nb_streams - 1;

	return true;
}

bool FileOutput::OpenOutputFile()
{

	CoreSettings* settings = core_engine_->GetSettings();
	AVOutputFormat* format = av_format_context_->oformat;
	int ret;
	AVDictionary* dict = NULL;

	if ((format->flags & AVFMT_NOFILE) == 0) {
		ret = avio_open2(&av_format_context_->pb, settings->GetOutputParam()->path, AVIO_FLAG_WRITE, NULL, &dict);
		if (ret < 0) {
			LOGGER_ERROR("avio_open2 failed");
			av_dict_free(&dict);
			return false;
		}
	}

	//strncpy(av_format_context_->url, settings->GetOutputParam()->path, std::string(settings->GetOutputParam()->path).length());
	//av_format_context_->filename[std::string(settings->GetOutputParam()->path).length() - 1] = 0;

	ret = avformat_write_header(av_format_context_, nullptr);
	if (ret < 0) {
		char errorbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		char* err = av_make_error_string(errorbuf, AV_ERROR_MAX_STRING_SIZE, ret);
		LOGGER_ERROR("avformat_write_header failed %s:%s", settings->GetOutputParam()->path, err);
		return false;
	}

	//if (av_dict_count(dict) > 0) {
	//	struct dstr str = { 0 };

	//	AVDictionaryEntry* entry = NULL;
	//	while ((entry = av_dict_get(dict, "", entry,
	//		AV_DICT_IGNORE_SUFFIX)))
	//		dstr_catf(&str, "\n\t%s=%s", entry->key, entry->value);

	//	blog(LOG_INFO, "Invalid muxer settings: %s", str.array);
	//	dstr_free(&str);
	//}

	av_dict_free(&dict);

	return true;
}

void FileOutput::SendOutputThreadImpl()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(output_mutex_);
		output_cond_.wait(lock, [&] {
			return av_packets_.size != 0 || !output_running_;
			});
		if (!output_running_ || av_packets_.size == 0)
			break;
		BaseEncoder::EncodedFrameData packet;
		if (av_packets_.size) {
			circlebuf_pop_front(&av_packets_, &packet, sizeof(struct BaseEncoder::EncodedFrameData));
			if (!send_sps_pps_) {
				if (!SendSPSPPS()) {
					LOGGER_ERROR("SendSPSPPS failed");
					break;
				}
			}
			if (SendFFMpegPacket(&packet, false) < 0) {
				LOGGER_ERROR("SendFFMpegPacket failed");
				break;
			}
		}
	}

	{
		std::unique_lock<std::mutex> lock(output_mutex_);
		FreeCirclePackets();
	}

	LOGGER_INFO("Released");
}

void FileOutput::FreeCirclePackets()
{
	while (av_packets_.size) {
		struct BaseEncoder::EncodedFrameData packet;
		circlebuf_pop_front(&av_packets_, &packet, sizeof(BaseEncoder::EncodedFrameData));
		if (packet.data)
		{
			free(packet.data);
			packet.data = nullptr;
		}
	}
}

bool FileOutput::SendSPSPPS()
{
	LOGGER_INFO("Enter");
	if (!video_encoder_)
		return false;
	uint8_t* header = nullptr;
	size_t headerSize = 0;
	struct BaseEncoder::EncodedFrameData packet;
	memset(&packet, 0, sizeof(BaseEncoder::EncodedFrameData));
	packet.keyframe = true;
	packet.bAudio = false;

	video_encoder_->GetEncoderExtraData(&header, &headerSize);
	if (!header)
		return false;

	packet.data = header;
	packet.size = headerSize;
	send_sps_pps_ = SendFFMpegPacket(&packet, true) >= 0;
	return send_sps_pps_;
}

int FileOutput::SendFFMpegPacket(struct BaseEncoder::EncodedFrameData* packet, bool is_header)
{
	AVPacket av_packet = { 0 };
	int ret = 0;
	int streamIdx = packet->bAudio ? 1 : 0;

	//2. send
	av_init_packet(&av_packet);
	av_packet.data = packet->data;
	av_packet.size = (int)packet->size;
	av_packet.stream_index = streamIdx;
	av_packet.pts = RescaleTs(packet->pts, streamIdx);
	av_packet.dts = RescaleTs(packet->dts, streamIdx);

	//LOGGER_INFO("%s in pts:%lld out pts:%lld", packet->bAudio ? "audio" : "video", packet->pts, av_packet.pts);

	if (packet->keyframe)
	{
		av_packet.flags = AV_PKT_FLAG_KEY;
		//LOGGER_INFO("%s keyframe pts:%lld rescale:%lld", packet->bAudio ? "audio" : "video", packet->pts, av_packet.pts);
	}
	ret = av_interleaved_write_frame(av_format_context_, &av_packet);
	if (ret < 0) {

	}
	if (packet->data)
	{
		free(packet->data);
		packet->data = nullptr;
	}
	return ret;
}

int64_t FileOutput::RescaleTs(int64_t val, int idx)
{
	CoreSettings* settings = core_engine_->GetSettings();

	//AVRational newbase = (AVRational)av_make_q(1, settings->GetOutputParam()->audio.samples_per_sec);

	AVStream* avstream = av_format_context_->streams[idx];
	AVRational oldbase = idx == 0 ? av_video_codec_context_->time_base : av_audio_codec_context_->time_base;
	AVRational newbase = avstream->time_base;

	const char* type = idx == 0 ? "video" : "audio";
	AVRational base = idx == 0 ? av_video_stream_->time_base : av_audio_streams_[0]->time_base;

	//LOGGER_INFO("%s obase[%d/%d] nbase[%d/%d]", type, oldbase.num, oldbase.den, newbase.num, newbase.den);

	return av_rescale_q_rnd(val / oldbase.num, oldbase, newbase,
		AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
}


