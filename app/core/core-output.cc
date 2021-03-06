#include "logger.h"
#include "core-output.h"
#include "core-settings.h"
#include "core-engine.h"
#include "core-display.h"
#include "core-scene.h"
#include "core-d3d.h"
#include "core-video.h"
#include "core-audio.h"
#include "platform.h"
#include "logger.h"
#include "x264-encoder.h"
#include "file-output.h"
#include "ffmpeg-aac-encoder.h"
#include "libyuv.h"

#define MICROSECOND_DEN 1000000
static inline int64_t packet_dts_usec(int64_t dts, int32_t den)
{
	return dts * MICROSECOND_DEN / den;
}

CoreOutput::CoreOutput()
{
	final_exit_.store(false);
}

CoreOutput::~CoreOutput()
{
	if (output_instance_)
		output_instance_.reset();
	if (video_encoder_)
		video_encoder_.reset();
	if (audio_encoder_)
		audio_encoder_.reset();

	if (encoded_video_list_.size())
	{
		LOGGER_INFO("Release video size:%d", encoded_video_list_.size());
		while (encoded_video_list_.size())
		{
			BaseEncoder::EncodedFrameData new_data = encoded_video_list_.front();
			encoded_video_list_.pop_front();
			if (new_data.data)
			{
				free(new_data.data);
				new_data.data = nullptr;
			}
		}
	}

	if (encoded_audio_list_.size())
	{
		LOGGER_INFO("Release audio size:%d", encoded_audio_list_.size());
		while (encoded_audio_list_.size())
		{
			BaseEncoder::EncodedFrameData new_data = encoded_audio_list_.front();
			encoded_audio_list_.pop_front();
			if (new_data.data)
			{
				free(new_data.data);
				new_data.data = nullptr;
			}
		}
	}
}

void CoreOutput::PreEndup()
{
	CoreVideo* video = core_engine_->GetVideo();
	CoreAudio* audio = core_engine_->GetAudio();
	final_exit_.store(true);
	{
		std::unique_lock<std::mutex> lock(encoded_data_mutex_);
		video_stopped_ts_ = video_highest_ts_;
		audio_stopped_ts_ = audio_highest_ts_;
	}
	video->UnRegisterVideoDataCallback(this);
	audio->UnRegisterCoreAudioDataCallback(this);
	if (init_thread_.joinable())
		init_thread_.join();
	video_raw_data_cond_.notify_one();
	if (video_thread_.joinable())
		video_thread_.join();
	audio_raw_data_cond_.notify_one();
	if (audio_thread_.joinable())
		audio_thread_.join();

	int size = encoded_audio_list_.size();

}

void CoreOutput::StartupCoreOutput()
{
	CoreSettings* settings = core_engine_->GetSettings();
	if (!settings->GetOutputParam()->enable)
		return;
	final_exit_.store(false);
	init_thread_ = std::thread(&CoreOutput::InitImpl, this);
}

void CoreOutput::InitData()
{
	std::unique_lock<std::mutex> lock(encoded_data_mutex_);
	encoded_video_offset_ = 0;
	encoded_audio_offset_ = 0;
	encoded_video_received_ = false;
	encoded_audio_received_ = false;
}

void CoreOutput::InitImpl()
{
	InitData();

	output_instance_.reset(new FileOutput());
	video_encoder_.reset(new X264Encoder());
	audio_encoder_.reset(new FFMpegAACEncoder());
	output_instance_->SetCoreEnv(core_engine_);
	video_encoder_->SetCoreEnv(core_engine_);
	audio_encoder_->SetCoreEnv(core_engine_);
	output_instance_->SetVideoEncoder(video_encoder_);
	output_instance_->SetAudioEncoder(audio_encoder_);

	if (!output_instance_->Init())
		return;

	if (!video_encoder_->Init())
		return;

	if (!audio_encoder_->Init())
		return;

	output_start_time_ = os_gettime_ns();
	CoreVideo* video = core_engine_->GetVideo();
	CoreAudio* audio = core_engine_->GetAudio();
	video_thread_ = std::thread(&CoreOutput::VideoImpl, this);
	audio_thread_ = std::thread(&CoreOutput::AudioImpl, this);
	video->RegisterVideoDataCallback(this, std::bind(&CoreOutput::VideoRawDataReceived, this, std::placeholders::_1));
	audio->RegisterCoreAudioDataCallback(this, std::bind(&CoreOutput::AudioRawDataReceived, this, std::placeholders::_1));
}

void CoreOutput::VideoImpl()
{
	CoreSettings* settings = core_engine_->GetSettings();
	int width = settings->GetOutputParam()->video.width;
	int height = settings->GetOutputParam()->video.height;
	for (;;)
	{
		uint8_t* output_data[MAX_AV_PLANES] = { 0 };
		size_t output_linesize[MAX_AV_PLANES] = { 0 };
		int output_plane = 0;
		bool encoder_receiveframe = false;

		bool popraw = false;
		CoreVideoData::RawData rawData;
		{
			std::unique_lock<std::mutex> lock(video_raw_data_mutex_);
			video_raw_data_cond_.wait(lock, [&] {
				return !video_raw_data_list_.empty() || final_exit_.load();
				});
			if (final_exit_.load() && video_raw_data_list_.empty())
			{
				return;
			}
			else
			{
				CoreVideoData::RawData temp = video_raw_data_list_.front();
				memcpy(&rawData, &temp, sizeof(CoreVideoData::RawData));
				video_raw_data_list_.pop_front();
				popraw = true;
			}
		}
		if (popraw)
		{
			CoverBgraToNeededFormat(rawData.bgra_data, rawData.linesize, output_data, output_linesize, &output_plane, width, height);
			video_encoder_->InputRawData(output_data, output_linesize, output_plane, rawData.timestamp, &encoder_receiveframe);

			if (encoder_receiveframe)
			{
				BaseEncoder::EncodedFrameData encodedData;
				video_encoder_->GetEncodedData(&encodedData);
				encodedData.ts_ns_ = packet_dts_usec(encodedData.dts, settings->GetOutputParam()->video.fps);
				if (encodedData.data && encodedData.size)
				{
					std::unique_lock<std::mutex> lock(encoded_data_mutex_);

					if (encodedData.ts_ns_ > video_highest_ts_)
						video_highest_ts_ = encodedData.ts_ns_;

					if (encoded_video_received_ && encoded_audio_received_)
					{
						encodedData.pts -= encoded_video_offset_;
						encodedData.dts -= encoded_video_offset_;
						BaseEncoder::EncodedFrameData new_data;
						DeepCopyEncodedData(&new_data, &encodedData);
						encoded_video_list_.push_back(new_data);
						if (encoded_video_list_.size())
						{
							while (encoded_video_list_.size())
							{
								BaseEncoder::EncodedFrameData new_data = encoded_video_list_.front();
								if (audio_highest_ts_ && new_data.ts_ns_ > audio_highest_ts_)
								{
									//LOGGER_INFO("video ts:%lld audio highest:%lld break", new_data.ts_ns_, audio_highest_ts_);
									encoded_video_list_.pop_front();
									break;
								}
								encoded_video_list_.pop_front();
								output_instance_->InputEncodedData(new_data);

								if (!encoded_video_send_)
									encoded_video_send_ = true;
								if (new_data.data)
								{
									free(new_data.data);
									new_data.data = nullptr;
								}
							}
						}
					}
					else
					{
						if (encodedData.keyframe && !encoded_video_received_)
						{
							encoded_video_received_ = true;
							encoded_video_offset_ = encodedData.pts;
						}
						if (encoded_video_received_)
						{
							encodedData.pts -= encoded_video_offset_;
							encodedData.dts -= encoded_video_offset_;
							BaseEncoder::EncodedFrameData new_data;
							DeepCopyEncodedData(&new_data, &encodedData);
							encoded_video_list_.push_back(new_data);
						}
					}
				}
			}
			if (rawData.bgra_data)
			{
				free(rawData.bgra_data);
				rawData.bgra_data = nullptr;
			}
			if (output_data)
			{
				if (output_data[0])
				{
					free(output_data[0]);
					output_data[0] = nullptr;
				}
			}
		}
	}
}

void CoreOutput::AudioImpl()
{
	CoreSettings* settings = core_engine_->GetSettings();
	for (;;)
	{
		uint8_t* output_data[MAX_AV_PLANES] = { 0 };
		size_t output_linesize[MAX_AV_PLANES] = { 0 };
		size_t linsize[MAX_AV_PLANES] = { 0 };
		int output_plane = 0;
		bool encoder_receiveframe = false;

		bool popraw = false;

		CoreAudioData::AudioMixerOutput rawData;
		{
			std::unique_lock<std::mutex> lock(audio_raw_data_mutex_);
			audio_raw_data_cond_.wait(lock, [&] {
				return !audio_raw_data_list_.empty() || final_exit_.load();
				});
			if (final_exit_.load() && audio_raw_data_list_.empty())
			{
				return;
			}
			else
			{
				CoreAudioData::AudioMixerOutput temp = audio_raw_data_list_.front();
				memcpy(&rawData, &temp, sizeof(CoreAudioData::AudioMixerOutput));
				audio_raw_data_list_.pop_front();
				popraw = true;
			}
		}

		if (popraw)
		{
			audio_encoder_->InputRawData(rawData.data, linsize, 1, rawData.timestamp - output_start_time_, &encoder_receiveframe);
			if (encoder_receiveframe)
			{
				BaseEncoder::EncodedFrameData encodedData;
				audio_encoder_->GetEncodedData(&encodedData);
				encodedData.ts_ns_ = packet_dts_usec(encodedData.dts, settings->GetOutputParam()->audio.samples_per_sec);
				if (encodedData.data && encodedData.size)
				{
					std::unique_lock<std::mutex> lock(encoded_data_mutex_);

					if (encodedData.ts_ns_ > audio_highest_ts_)
						audio_highest_ts_ = encodedData.ts_ns_;

					if (encoded_video_send_ && encoded_audio_received_)
					{
						encodedData.pts -= encoded_audio_offset_;
						encodedData.dts -= encoded_audio_offset_;
						BaseEncoder::EncodedFrameData new_data;
						DeepCopyEncodedData(&new_data, &encodedData);
						encoded_audio_list_.push_back(new_data);

						if (encoded_audio_list_.size())
						{
							while (encoded_audio_list_.size())
							{
								BaseEncoder::EncodedFrameData new_data = encoded_audio_list_.front();
								if (video_highest_ts_ && new_data.ts_ns_ > video_highest_ts_)
								{
									break;
								}

								encoded_audio_list_.pop_front();
								output_instance_->InputEncodedData(new_data);

								if (new_data.data)
								{
									free(new_data.data);
									new_data.data = nullptr;
								}
							}
						}
					}
					else
					{
						if (!encoded_audio_received_)
						{
							encoded_audio_received_ = true;
							encoded_audio_offset_ = encodedData.dts;
						}
						encodedData.pts -= encoded_audio_offset_;
						encodedData.dts -= encoded_audio_offset_;
						BaseEncoder::EncodedFrameData new_data;
						DeepCopyEncodedData(&new_data, &encodedData);
						encoded_audio_list_.push_back(new_data);
					}
				}
			}
			for (int i = 0; i < MAX_AV_PLANES; i++)
			{
				if (rawData.data[i])
				{
					free(rawData.data[i]);
				}
			}
		}
	}
}

void CoreOutput::AudioRawDataReceived(const CoreAudioData::AudioMixerOutput* data)
{
	if (final_exit_.load())
		return;
	std::unique_lock<std::mutex> lock(audio_raw_data_mutex_);
	CoreAudioData::AudioMixerOutput rawData;
	rawData.format = data->format;
	rawData.frames = data->frames;
	rawData.samples_per_sec = data->samples_per_sec;
	rawData.speakers = data->speakers;
	rawData.timestamp = data->timestamp;
	int channels = CoreAudioData::get_audio_channels(rawData.speakers);
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
	{
		rawData.data[i] = nullptr;
		if (i < channels)
		{
			rawData.data[i] = (uint8_t*)malloc(data->frames * sizeof(float));
			memcpy(rawData.data[i], data->data[i], data->frames * sizeof(float));
		}
	}
	audio_raw_data_list_.push_back(rawData);
	audio_raw_data_cond_.notify_one();
}

void CoreOutput::VideoRawDataReceived(const CoreVideoData::RawData* data)
{
	if (final_exit_.load())
		return;
	std::unique_lock<std::mutex> lock(video_raw_data_mutex_);
	CoreVideoData::RawData rawData;
	rawData.linesize = data->linesize;
	rawData.timestamp = data->timestamp;
	rawData.bgra_data = (uint8_t*)malloc(data->linesize);
	memset(rawData.bgra_data, 0, data->linesize);
	memcpy(rawData.bgra_data, data->bgra_data, data->linesize);
	video_raw_data_list_.push_back(rawData);
	video_raw_data_cond_.notify_one();
}

void CoreOutput::DeepCopyEncodedData(BaseEncoder::EncodedFrameData* dst, const BaseEncoder::EncodedFrameData* src)
{
	if (dst->data)
	{
		free(dst->data);
		dst->data = nullptr;
	}
	dst->data = (uint8_t*)malloc(src->size);
	memset(dst->data, 0, src->size);
	memcpy(dst->data, src->data, src->size);
	dst->size = src->size;
	dst->pts = src->pts;
	dst->dts = src->dts;
	dst->keyframe = src->keyframe;
	dst->bAudio = src->bAudio;
	dst->ts_ns_ = src->ts_ns_;
}

void CoreOutput::CoverBgraToNeededFormat(uint8_t* input_data, size_t input_linesize,
	uint8_t** output_data, size_t* output_linesize, int* plane, int width, int height)
{
	size_t size;
	size_t offsets[MAX_AV_PLANES];
	int alignment = 32;

	size = width * height;
	offsets[0] = size;
	const uint32_t half_width = (width + 1) / 2;
	const uint32_t half_height = (height + 1) / 2;
	const uint32_t quarter_area = half_width * half_height;
	size += quarter_area;
	offsets[1] = size;
	size += quarter_area;
	output_data[0] = (uint8_t*)malloc(size);
	output_data[1] = (uint8_t*)output_data[0] + offsets[0];
	output_data[2] = (uint8_t*)output_data[0] + offsets[1];
	output_linesize[0] = width;
	output_linesize[1] = half_width;
	output_linesize[2] = half_width;

	*plane = 3;

	libyuv::ARGBToI420(input_data, width * 4,
		output_data[0], output_linesize[0],
		output_data[1], output_linesize[1],
		output_data[2], output_linesize[2],
		width, height);
}